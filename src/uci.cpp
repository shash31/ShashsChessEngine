#pragma GCC diagnostic ignored "-Wcomment" 

#include <thread>
#include <fstream>
#include <ranges>
#include <unordered_map>
#include "search.h"

/*
TODOs:
- Bugs
    - Use puzzles for sanity-testing
    - Endgame issues
    - Attacking/Sacrificing issues

- Features to add
    - SEE(Static Exchange Evaluation)
    - Internal iterative reduction
    - Futility pruning and late move pruning

- Potential additions
    - Look into using NNUE as evaluation instead
    - Look into multithreaded search


Compile commands:
    - Production build: clang++ -std=c++23 -O3 -flto -mcpu=native src/*.cpp -o myengine
    - WASM build: "em++ src/ChessEngine/*.cpp -std=c++23 -O3 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORTED_FUNCTIONS=_get_best_move -s EXPORTED_RUNTIME_METHODS=ccall -o src/ChessEngine.js && mv src/ChessEngine.wasm public/"
    - Debug/building compile(slow only use to catch bugs): clang++ -std=c++23 -O0 -g -Wall -Wextra -fno-omit-frame-pointer -fsanitize=address,undefined src/*.cpp -o myengine_san
    - Catch uninitialized variables (add to the debug build): -ftrivial-auto-var-init=pattern
    - // Profiling (to see where time goes, e.g. in Instruments): clang++ -std=c++23 -O2 -g -fno-omit-frame-pointer src/*.cpp -o myengine_prof
    - Data races, if you add threads: -O1 -g -fsanitize=thread. This can’t be combined with the address sanitizer.

Testing commands:
    - caffeinate -di ./sprt.sh 2>&1 | tee sprt_log.txt
*/

std::thread searchThread;

constexpr const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr const char* TEST_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
    "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1",
    "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
};
// Interesting positions:

// r1bqr1k1/p1p2ppp/1p2p3/b2P4/Q1Pn4/B1N3P1/P2P1PBP/R4RK1 w - - 0 15 ; Moves: d5e6 c8e6
// Sacking the exchange is good for black as white's kingside light squares are too weak after losing its light squared bishop 

void clearTablesAndHistory() {
    tt.clear();
    current_age = 0;
    killer_moves = {};
    history_moves = {};
}

void initialize() {
    tt.resize(256); // 256 MB
    zobrist_keys.init();
    init_tables();
    calc_lmr_table(); // For late move reduction formulas
}

// Find the legal move matching a UCI string like "e2e4" or "e7e8q".
Move parseMove(Board& board, const std::string& str) {
    MoveList moves;
    board.generate_pseudo_legal_moves(moves, false);
    for (int i = 0; i < moves.size(); i++) { 
        if (moveToString(moves[i]) == str) return moves[i];
    }
    return Move{};  // "null" move: not found / illegal
}

void parsePosition(Board& board, std::istringstream& is) {
    std::string token;
    std::string fen;

    is >> token;
    if (token == "startpos") {
        fen = START_FEN;
        token.clear();
        is >> token;                       // "moves" or nothing
    } else if (token == "fen") {
        // A FEN is up to 6 space-separated fields; read until "moves" or end.
        while (is >> token && token != "moves") {
            if (!fen.empty()) fen += ' ';
            fen += token;
        }
    } else {
        return;                            // malformed, ignore
    }

    board.reset();
    board.parseFEN(fen);                   // std::string converts to string_view

    if (token == "moves") {
        while (is >> token) {
            Move m = parseMove(board, token);
            if (m == 0) break;       // adapt to your null-move check
            board.make_move(m);
        }
    }
}

void parseEval(Board& board, std::istringstream& is) {
    std::string token;
    std::string fen;

    // A FEN is up to 6 space-separated fields; read until "moves" or end.
    while (is >> token) {
        if (!fen.empty()) fen += ' ';
        fen += token;
    }

    board.reset();
    board.parseFEN(fen);

    int eval = pesto_eval(board);
    std::println("{}", eval); std::fflush(stdout);
}

void startSearch(Board& board) {
    if (searchThread.joinable()) searchThread.join();
    stopSearch = false;
    searchThread = std::thread([&board] {
        Move best = ids(board); // prints info lines
        std::println("bestmove {}", moveToString(best)); std::fflush(stdout);
    });
}

// Parse arguments and set soft and hard time limits
void parseGo(Board& board, std::istringstream& is) {
    std::string token;
    int wtime = 0;
    int btime = 0;
    int winc = 0;
    int binc = 0;
    int movestogo = std::max(20, 50 - board.fullmove_no);
    soft_limit_ms = 0;
    hard_limit_ms = 0;
    depth_limit = -1;
    while (is >> token) {
        if (token == "wtime") {
            is >> token;
            wtime = std::stoi(token);
        }
        else if (token == "btime") {
            is >> token;
            btime = std::stoi(token);
        }
        else if (token == "winc") {
            is >> token;
            winc = std::stoi(token);
        }
        else if (token == "binc") {
            is >> token;
            binc = std::stoi(token);
        }
        else if (token == "movetime") {
            is >> token;
            soft_limit_ms = std::stoi(token);
            hard_limit_ms = std::stoi(token);
        }
        else if (token == "movestogo") {
            is >> token;
            movestogo = std::stoi(token);
        }
        else if (token == "depth") {
            is >> token;
            depth_limit = std::stoi(token);
            soft_limit_ms = 300000; // Both set to 5 minutes
            hard_limit_ms = 300000;
        }
        else if (token == "nodes") {

        }
        else if (token == "infinite") {

        }
    }

    /* Time allocation formula
        overhead   = ~50-100          // network/process lag buffer
        movesLeft  = movestogo if given, else an estimate (e.g. 25-40)
        base       = (timeLeft - overhead) / movesLeft
        soft       = base + increment * ~0.75
        hard       = min(soft * 3-5, timeLeft * ~0.2)   // never close to your whole clock
    */
    if ((soft_limit_ms == 0) && (hard_limit_ms == 0)) {
        int timeLeft;
        int increment;
        if (board.turn == WHITE) {
            timeLeft = wtime; increment = winc;
        } else {
            timeLeft = btime; increment = binc;
        }
        int base = (timeLeft - 100) / movestogo;
        soft_limit_ms = base + increment * 0.75;
        hard_limit_ms = std::min(soft_limit_ms*4, timeLeft*0.2);
    }
    // std::println(stderr, "Soft limit: {}; Hard limit: {}", soft_limit_ms, hard_limit_ms); std::fflush(stderr);

    startSearch(board);
}

void stopAndJoin() {
    stopSearch = true;
    if (searchThread.joinable()) searchThread.join();
}

void parseSetOption(std::istringstream& is) {
    std::string option;
    is >> option;
    if (option == "Hash") {
        is >> option;
        tt.resize(std::stoi(option));
    }
}

uint64_t perft(int depth, Board& board) { // For testing correctness of move generation
    if (depth == 0) return 1ULL;

    uint64_t nodes = 0;
    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);

    for (int i = 0; i < move_list.count; ++i) {
        Move move = move_list[i];

        board.make_move(move);
        if (board.is_king_capturable()) {
            board.unmake_move(move); 
            continue;
        }

        nodes += perft(depth - 1, board);

        board.unmake_move(move);
    }

    return nodes;
}

void perft_divide(int depth, Board& board) {
    uint64_t total_nodes = 0;

    auto start = std::chrono::steady_clock::now();
    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);

    for (int i = 0; i < move_list.count; ++i) {
        Move move = move_list[i];

        board.make_move(move);
        if (board.is_king_capturable()) {
            board.unmake_move(move);
            continue;
        }

        uint64_t nodes = perft(depth - 1, board);
        total_nodes += nodes;
        board.unmake_move(move);

        uint8_t to = (move >> 6) & 0x3F;
        uint8_t from = move & 0x3F;
        std::println("{}{}: {}", getStringFromSquare(from), getStringFromSquare(to), nodes);
    }

    auto end = std::chrono::steady_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double nps = total_nodes*1000/ std::max<int64_t>(seconds, 1);

    std::cout << "\nTotal Nodes at Depth " << depth << ": " << total_nodes << "\n";
    std::println("NPS: {}", nps);
    std::fflush(stdout);
}

void benchmark_testing(Board& board, int user_depth_limit) {
    stopAndJoin();                      // clean up any earlier search thread first
    uint64_t total_bench_nodes = 0;
    auto t0 = std::chrono::steady_clock::now();

    for (const std::string& fen : TEST_FENS) { 
        clearTablesAndHistory();
        board.reset();
        board.parseFEN(fen);

        depth_limit   = user_depth_limit;
        soft_limit_ms = hard_limit_ms = 300000;
        stopSearch    = false;

        Move move = ids(board);
        std::println("bestmove {}", moveToString(move)); std::fflush(stdout);
        total_bench_nodes += total_nodes;
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::println("Nodes searched: {}", total_bench_nodes);
    std::println("Time: {} ms", ms);
    std::println("NPS: {}", total_bench_nodes * 1000 / std::max<int64_t>(ms, 1));
    std::fflush(stdout);
}

void test_puzzles(Board& board, int npuzzles, std::string theme) {
    stopAndJoin();                      // clean up any earlier search thread first

    // uint64_t total_bench_nodes = 0;
    // auto t0 = std::chrono::steady_clock::now();

    // std::ifstream puzzle_db("puzzles.txt");

    // std::string line;
    // int incorrect = 0;
    
    // while (std::getline(puzzle_db, line)) {
    // // for (int a = 0; a < 10; a++) {
    //     // std::getline(puzzle_db, line);
    //     std::array<std::string, 8> current_puzzle;
    //     current_puzzle[0] = line;
    //     std::println("{}", line);
    //     for (int i = 1; i < 8; i++) {
    //         std::getline(puzzle_db, line);
    //         current_puzzle[i] = line;
    //         std::println("{}", line);
    //     }

    //     std::istringstream fenline(current_puzzle[1]);
    //     std::string fen;
    //     fenline >> fen >> fen;
    //     std::string token;
    //     fen += " ";
    //     while (fenline >> token) fen += token + " ";
    //     // std::println("FEN: {}", fen);

    //     clearTablesAndHistory();
    //     board.reset();
    //     board.parseFEN(fen);

    //     std::istringstream moves(current_puzzle[2]);
    //     std::string move;
    //     moves >> move >> move;
    //     // std::println("First move: {}", move);

    //     // Making first move of puzzle
    //     Move m = parseMove(board, move);
    //     // std::println("Parsed move: {}", moveToString(m));
    //     board.make_move(m);

    //     depth_limit = 15;
    //     soft_limit_ms = hard_limit_ms = 5000;
    //     stopSearch = false;

    //     std::println("Searching");
    //     Move result = ids(board);
    //     std::println("bestmove {}", moveToString(result)); 
    //     total_bench_nodes += total_nodes;

    //     // Checking answer
    //     moves >> move;
    //     // std::println("Answer: {}", move);
    //     if (moveToString(result) != move) {
    //         incorrect++;
    //         std::println("INCORRECT ANSWER");
    //     }

    //     std::println();
    //     std::println();

    //     while (line != "") std::getline(puzzle_db, line);
    // }

    // // Close the file stream
    // puzzle_db.close();

    // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    //               std::chrono::steady_clock::now() - t0).count();
    // std::println("Nodes searched: {}", total_bench_nodes);
    // std::println("Time: {} ms", ms);
    // std::println("NPS: {}", total_bench_nodes * 1000 / std::max<int64_t>(ms, 1));
    
    // std::println();
    // std::println("Total correct: {}/{}", npuzzles - incorrect, npuzzles);

    // Extracting incorrect puzzles
    // std::ifstream results("results2.txt");
    // std::ofstream incorrect_puzzles("incorrect_puzzles.txt");

    // std::string line;
    // std::string current_puzzle = "";
    // while(std::getline(results, line)) {
    //     current_puzzle += line + "\n";
    //     if (line.rfind("Puzzle ID:", 0) == 0) {
    //         current_puzzle = line + "\n";
    //     } else if (line == "INCORRECT ANSWER") {
    //         incorrect_puzzles << current_puzzle << "\n";
    //     }
    // }

    std::ifstream incorrect_puzzles("incorrect_puzzles.txt");
    if (!incorrect_puzzles) {
        std::cerr << "Error: Could not open file!" << std::endl;
    }

    // Getting stats of incorrect puzzles to evaluate weaknesses of engine
    std::array<int, 256> ratings{};
    std::unordered_map<std::string, int> theme_counts;
    int ratings_sum = 0; // To calculate average rating of incorrect puzzles
    int min_rating = 100000; // For now
    int max_rating = 0;

    std::string line;
    while (std::getline(incorrect_puzzles, line)) {
        if (line.rfind("Rating:", 0) == 0) {
            std::istringstream is(line);
            std::string r;
            is >> r >> r; // Skipping "Rating: "
            int rating = std::stoi(r);
            ratings_sum += rating;
            if (rating < min_rating) min_rating = rating;
            if (rating > max_rating) max_rating = rating;
        } else if (line.rfind("Themes:", 0) == 0) {
            std::istringstream is(line);
            std::string theme;
            is >> theme; // Skipping "Themes: "
            while (is >> theme) theme_counts[theme]++;
        }
    }

    incorrect_puzzles.close();

    std::println("Average rating of incorrect puzzle: {}", static_cast<double>(ratings_sum) / 175.0);
    std::println("Lowest rated puzzle: {}", min_rating);
    std::println("Highest rated puzzle: {}", max_rating);

    // For seeing which are the most common themes it misses
    std::vector<std::pair<std::string, int>> sorted_theme_counts(theme_counts.begin(), theme_counts.end());

    std::sort(sorted_theme_counts.begin(), sorted_theme_counts.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // Sorts by the 'int' value descending
    });

    std::cout << "--- Full Sorted List of theme counts---\n";
    for (const auto& [word, count] : sorted_theme_counts) {
        std::cout << word << ": " << count << "\n";
    }
    
    std::fflush(stdout);
}

void test_new_puzzles(Board& board, int npuzzles, std::string theme) {
    uint64_t total_bench_nodes = 0;
    auto t0 = std::chrono::steady_clock::now();

    std::ifstream puzzle_db("lichess_db_puzzle.csv");

    if (!puzzle_db.is_open()) {
        std::cerr << "Error: Could not open the file." << std::endl;
    }

    std::string line;
    // Skipping all these puzzles cus we've covered them
    for (int i = 0; i < 13091; i++) {
         std::getline(puzzle_db, line);
    }

    int incorrect = 0;

    // File format: PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes
    for (int i = 0; i < npuzzles; i++) {
        std::getline(puzzle_db, line);
        std::stringstream ss(line);
        std::string field;
        std::array<std::string, 8> row;

        // Only getting relevant fields
        for (int j = 0; j < 8; j++) {
            std::getline(ss, field, ',');
            row[j] = field;
        }

        std::println("Puzzle ID: {}", row[0]);
        std::println("FEN: {}", row[1]);
        std::println("Moves: {}", row[2]);
        std::println("Rating: {}", row[3]);
        std::println("Rating deviation: {}", row[4]);
        std::println("Popularity: {}", row[5]);
        std::println("No. of plays: {}", row[6]);
        std::println("Themes: {}", row[7]);

        clearTablesAndHistory();
        board.reset();
        board.parseFEN(row[1]);

        std::istringstream moves(row[2]);
        std::string move;
        moves >> move;

        // Making first move of puzzle
        Move m = parseMove(board, move);
        board.make_move(m);

        depth_limit = 15;
        soft_limit_ms = hard_limit_ms = 5000;
        stopSearch = false;

        std::println("Searching");
        Move result = ids(board);
        std::println("bestmove {}", moveToString(result)); 
        total_bench_nodes += total_nodes;
        
        // Checking answer
        moves >> move;
        if (moveToString(result) != move) {
            incorrect++;
            std::println("INCORRECT ANSWER");
        }

        std::println();
        std::println();
    }

    // Close the file stream
    puzzle_db.close();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::println("Nodes searched: {}", total_bench_nodes);
    std::println("Time: {} ms", ms);
    std::println("NPS: {}", total_bench_nodes * 1000 / std::max<int64_t>(ms, 1));
    
    std::println();
    std::println("Total correct: {}/{}", npuzzles - incorrect, npuzzles);
    
    std::fflush(stdout);
}

int main(int argc, char* argv[]) {
    initialize();
    Board board;
    // Board board2;
    // std::string test_fen = "r2qk1nr/pppb2pp/2nb1p2/3pp3/3P1B2/2P1PN1P/PP3PP1/RN1QKB1R w KQkq - 1 7";

    // board.parseFEN(test_fen);
    // board2.parseFEN(test_fen);
    // std::println("SEE for d4e5: {}", board.see_ge(makeMoveU16(D4, E5, CAPTURE), 0));
    // std::println("SEE for f4e5: {}", board.see_ge(makeMoveU16(F4, E5, CAPTURE), 0));
    // std::println("SEE for f3e5: {}", board.see_ge(makeMoveU16(F3, E5, CAPTURE), 0));
    // std::println("{}", board == board2);
    // return 0;

    if (argv[1] && std::string(argv[1]) == "bench") {
        int d = 15;
        if ((argc == 4) && std::string(argv[2]) == "-d") {
            d = std::stoi(argv[3]);
        }
        benchmark_testing(board, d);
        return 0;
    } else if (argv[1] && std::string(argv[1]) == "test") {
        int npuzzles = 20;
        std::string theme = "";
        for (int i = 2; i < argc - 1; i++) {
            if (std::string(argv[i]) == "-n") {
                npuzzles = std::stoi(argv[i+1]);
                i++;
            } else if (std::string(argv[i]) == "-t") {
                theme = argv[i+1];
                i++;
            }
        }

        test_puzzles(board, npuzzles, theme);

        return 0;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        std::string cmd; is >> cmd;
        if (cmd == "uci") {
            std::cout << "id name MyC++Engine\n";
            std::cout << "id author Shashwat Dudeja\n";
            std::cout << "option name Hash type spin default 256 min 1 max 4096\n";
            std::cout << "uciok" << std::endl;
        } 
        else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        }
        else if (cmd == "ucinewgame") {
            clearTablesAndHistory();
        }
        else if (cmd == "position") {
            stopAndJoin();
            parsePosition(board, is);
        } 
        else if (cmd == "go") {
            stopAndJoin();
            parseGo(board, is);
        } 
        else if (cmd == "setoption") {
            parseSetOption(is);
        }
        else if (cmd == "stop") {
            stopAndJoin();
        }
        else if (cmd == "quit") {
            stopAndJoin();
            break;
        }
        else if (cmd == "perft") {
            std::string token;
            is >> token;
            int depth = std::stoi(token);
            perft_divide(depth, board);
        }
        else if (cmd == "bench") {
            int d = 15;
            std::string token;
            if (is >> token) {
                if (token == "-d") {
                    is >> token;
                    d = std::stoi(token);
                }
            }
            benchmark_testing(board, d);
        }
        else if (cmd == "eval") {
            parseEval(board, is);
        }
    }
    return 0;
}
