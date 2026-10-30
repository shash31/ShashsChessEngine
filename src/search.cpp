#include "search.h"

inline void check_time() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    if (elapsed >= hard_limit_ms) {
        stopSearch = true;
    }
}

int quiescence(int ply, int alpha, int beta, Board& board) {
    nodes++;

    // Check time every 2048 nodes
    if ((nodes & 2047) == 0) check_time();

    // If time expired, abort search immediately
    if (stopSearch) return 0;

    // Checking if position is repeated
    int limit = std::min(board.halfmove_clock, board.ply_counter) / 2;
    for (int i = 0; i < limit; i++) {
        if (board.hash == board.history[board.ply_counter - 2 - 2*i].hash) return 0;
    }

    int original_alpha = alpha;
    Move tt_move = 0;
    int tt_score = 0;

    if (tt.probe(board.hash, 0, alpha, beta, tt_score, tt_move, ply)) return tt_score;

    Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
    bool in_check = board.is_square_attacked(king_pos);

    int stand_pat = -MATE;
    if (!in_check) {
        stand_pat = pesto_eval(board);
        if (stand_pat >= beta) return beta;
        if (alpha < stand_pat) alpha = stand_pat;
    }

    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list, !in_check); // ALL moves if in check, ONLY captures if quiet
    board.score_moves(move_list, tt_move, ply);

    int legal_moves_count = 0;
    Move best_move = 0;
    int best_score = in_check ? -MATE : stand_pat;

    for (int i = 0; i < move_list.count; i++) {
        // Selection sort move ordering
        int best_index = i;
        for (int j = i + 1; j < move_list.count; j++) {
            if (move_list.scores[j] > move_list.scores[best_index]) best_index = j;
        }
        std::swap(move_list[i], move_list[best_index]);
        std::swap(move_list.scores[i], move_list.scores[best_index]);

        Move move = move_list[i];

        // Delta Pruning: Skip bad captures if not in check and can't reach alpha
        if (!in_check && ((move >> 12) < 8)) { // If not in check & not a promotion
            int captured_val;
            if ((move >> 12) & 0x1) { // En passant
                captured_val = material_values[PAWN];
            } else {
                captured_val = material_values[char_to_piece(board.grid[(move >> 6) & 0x3F])];
            }
            if (stand_pat + captured_val + 200 < alpha) continue;
        }

        board.make_move(move);
        if (!board.is_king_capturable()) {
            legal_moves_count++;
            int score = -quiescence(ply + 1, -beta, -alpha, board);
            board.unmake_move(move);

            if (score > best_score) {
                best_score = score;
                best_move = move;

                if (score > alpha) alpha = score;
                if (alpha >= beta) break; // Cutoff
            }
        } else {
            board.unmake_move(move);
        }
    }

    if (in_check && legal_moves_count == 0) return -MATE + ply; // Actual Checkmate

    // Checking for 50-move draw after checkmate
    if (board.halfmove_clock >= 100) return 0;

    if (!stopSearch) {
        TTFlag flag = (best_score <= original_alpha) ? UPPER : (best_score >= beta ? LOWER : EXACT);
        tt.store(board.hash, 0, best_score, flag, best_move, ply);
    }

    return best_score;
}

int negamax(int depth, int ply, Board& board, int alpha, int beta) {
    nodes++;

    // Check time every 2048 nodes
    if ((nodes & 2047) == 0) check_time();

    // If time expired, abort search immediately
    if (stopSearch) return 0;

    // Checking if position is repeated
    int limit = std::min(board.halfmove_clock, board.ply_counter) / 2;
    for (int i = 0; i < limit; i++) {
        if (board.hash == board.history[board.ply_counter - 2 - 2*i].hash) return 0;
    }

    Move tt_move = 0;
    int tt_score = 0;
    if (tt.probe(board.hash, depth, alpha, beta, tt_score, tt_move, ply) && (ply > 0)) return tt_score;

    // Checking if in check
    Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
    bool in_check = board.is_square_attacked(king_pos);
    if (in_check) depth++; // Increase depth if in check

    if (depth == 0) return quiescence(ply + 1, alpha, beta, board);

    int static_eval = pesto_eval(board);

    // Reverse Futility Pruning
    // Conditions: Not in check, not a PV node, depth <= 5, tt move does exist and not a capture, beta isn't mate bound, not root node, static_eval - margin >= beta
    int margin = 100 * depth;
    if (!in_check && ply > 0 && (beta - alpha) <= 1 && depth <= 6
        && std::abs(beta) < 30000
        && (tt_move == 0 || (tt_move >> 12) < 4)
        && static_eval - 100 * depth >= beta)
        return (static_eval + beta) / 2;

    // Null move pruning
    // Conditions: Not in check, not a PV node, depth >= 3, ply > 0, has non-pawn material, static_eval >= beta
    if (!in_check && ((beta - alpha) <= 1) && depth >= 3 && ply > 0 && static_eval >= beta) {
        // Checking for non-pawn material
        bool non_pawn_material = false;
        for (int pt = KNIGHT; pt <= QUEEN; pt++) {
            if (board.piece_type_bb[board.turn][pt]) {
                non_pawn_material = true;
                break;
            }
        }
        if (non_pawn_material) {
            // Make null move
            board.make_null_move();
            
            int null_score = -negamax(depth - 3, ply + 1, board, -beta, -(beta - 1));

            // Unmake null move
            board.unmake_null_move();

            if (null_score >= beta) return null_score;
        }
    }

    MoveList move_list;
    board.generate_pseudo_legal_moves(move_list);
    board.score_moves(move_list, tt_move, ply);

    // For adding penalties to non cutoff quiet moves
    // Storing moving piece(3 bits (8..6 bits)) and to square(6 bits (5..0))
    std::array<Move, 256> quiets;
    int nq = 0;
    
    Move best_move = 0;
    int original_alpha = alpha;
    int best_score = -MATE;
    int legal_moves_count = 0;

    for (int i = 0; i < move_list.count; i++) {
        // Selection sort move ordering
        int best_index = i;
        for (int j = i + 1; j < move_list.count; j++) {
            if (move_list.scores[j] > move_list.scores[best_index]) best_index = j;
        }

        std::swap(move_list[i], move_list[best_index]);
        std::swap(move_list.scores[i], move_list.scores[best_index]);

        Move move = move_list[i];

        board.make_move(move);
        if (!board.is_king_capturable()) {
            legal_moves_count++;

            int to_sq = (move >> 6) & 0x3F;
            PieceType moving_piece = char_to_piece(board.grid[to_sq]); // As move has been made, moving piece is at target square

            // Principal Variation Search(PVS)
            // Hoping first move is best because of good move ordering
            int score;
            if (legal_moves_count == 1) { // First move (Full depth & window search)
                score = -negamax(depth - 1, ply + 1, board, -beta, -alpha);
            } else {
                Square enemy_king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING])); // as make_move is made, we find enemy king with board.turn instead of oppSide
                // Conditions for Late Move Reduction (LMR)
                // Depth >= 3, later move, not a capture or promotion, not in check, doesn't give check, not in pv node, not a killer move
                if (depth >= 3 && legal_moves_count >= 4 && (move >> 12) < CAPTURE && 
                    !in_check && (beta - alpha) <= 1 && move_list.scores[i] < 800'000 && !board.is_square_attacked(enemy_king_pos)) {
                    // Searching with a reduced depth
                    int reduction = lmr_table[std::min(depth, 63)][std::min(legal_moves_count, 63)];
                    int reduced_depth = std::max(1, depth - 1 - reduction);
                    score = -negamax(reduced_depth, ply + 1, board, -alpha - 1, -alpha);
                } else score = alpha + 1;

                if (score > alpha) {
                    // Re-searching at full depth but with narrow window
                    score = -negamax(depth - 1, ply + 1, board, -alpha - 1, -alpha);

                    // Full-window re-search required
                    if ((score > alpha) && (score < beta)) {
                        score = -negamax(depth - 1, ply + 1, board, -beta, -alpha);
                    }
                }
            }

            if (stopSearch) { // Returning before corrupted score can affect alpha, beta, killer, history, etc.
                board.unmake_move(move);
                return 0;
            }

            if (score > best_score) {
                best_score = score;
                best_move = move;
            }
            if (score > alpha) {
                // if (((move >> 12) & 0x4) == 0) history_moves[char_to_piece(board.grid[move & 0x3F])][(move >> 6) & 0x3F] += depth; // Store if quiet move
                alpha = score;
            }

            if (score >= beta) { // Beta-cutoff (Opponent will never let us get here)
                board.unmake_move(move);
                if ((move >> 12 & 0x4) == 0) { // Store if quiet move
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = move;
                    
                    int bonus = std::min(depth * depth, 1600);
                    int h = history_moves[board.oppSide][moving_piece][to_sq]; // As move has been made, turn and oppSide flip

                    history_moves[board.turn][moving_piece][to_sq] += bonus * (MAX_HISTORY - h)/MAX_HISTORY;
                    
                    // Adding penalties to quiet moves that didn't cause cutoff
                    for (int j = 0; j < nq; j++) {
                        int pmoving_piece = quiets[j] >> 6;
                        int pto_sq = quiets[j] & 0x3F;
                        h = history_moves[board.turn][pmoving_piece][pto_sq];
                        history_moves[board.turn][pmoving_piece][pto_sq] -= bonus * (MAX_HISTORY + h)/MAX_HISTORY; 
                    }
                }

                break;
            } else { // Storing quiet moves that didn't cause cutoff
                if ((move >> 12 & 0x4) == 0) quiets[nq++] = (moving_piece << 6) | (to_sq);
            }
        }

        board.unmake_move(move);
    }

    if (legal_moves_count == 0) {
        Square king_pos = static_cast<Square>(std::countr_zero(board.piece_type_bb[board.turn][KING]));
        if (board.is_square_attacked(king_pos)) { // in check
            return -MATE + ply; // Checkmate, return negative score with depth to prefer faster checkmates
        }
        return 0; // stalemate
    }

    // Checking for 50-move draw after checkmate
    if (board.halfmove_clock >= 100) return 0;

    if (!stopSearch) {
        TTFlag flag;
        if (best_score <= original_alpha) {
            flag = UPPER; // Fail-low
        } else if (best_score >= beta) {
            flag = LOWER; // Fail-high
        } else {
            flag = EXACT; // PV Node
        }

        tt.store(board.hash, depth, best_score, flag, best_move, ply);
    }

    return best_score;
}


Move ids(Board& board) {
    current_age++;

    total_nodes = 0;

    // Aging entries in history table
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 6; j++) {
            for (int k = 0; k < 64; k++) {
                history_moves[i][j][k] /= 2;
            }
        }
    }

    // Iterative Deepening Search
    start_time = std::chrono::steady_clock::now();
    auto totalTime = 0;

    MoveList moves;
    board.generate_pseudo_legal_moves(moves, false);
    Move bestmove;
    // Setting bestmove to first legal move in case of time trouble as fallback
    for (int i = 0; i < moves.size(); i++) {
        board.make_move(moves[i]);
        if (!board.is_king_capturable()) {
            bestmove = moves[i];
            board.unmake_move(moves[i]);
            break;
        }
        board.unmake_move(moves[i]);
    }
    int alpha = -MATE;
    int beta = MATE;
    int score;

    int max_depth = 30;
    if (depth_limit != -1) max_depth = depth_limit + 1;
    for (int depth = 1; depth < max_depth; depth++) { 
        auto start = std::chrono::steady_clock::now();
        nodes = 0;
        
        if (depth <= 7) { // Search with full window at earlier depths as search tree is small anyway
            score = negamax(depth, 0, board, -MATE, MATE);
        } else {
            // Aspiration window margin
            int margin = 50;
            alpha = score - margin;
            beta = score + margin;

            while (true) {
                score = negamax(depth, 0, board, alpha, beta);
                
                if (score <= alpha) {
                    alpha -= margin;
                    margin = static_cast<int>(margin * 2.5); // Expand margin so the next re-search window is wider
                } else if (score >= beta) {
                    beta += margin;
                    margin = static_cast<int>(margin * 2.5); // Expand margin so the next re-search window is wider
                } else break; // Successful search
            }
        }

        if (stopSearch) break; // Time ran out; Discard results

        // Retrieve best move found so far from TT for the root position
        Move tt_move = 0;
        int dummy_score = 0;
        tt.probe(board.hash, depth, alpha, beta, dummy_score, tt_move, 0);
        
        if (tt_move != 0) bestmove = tt_move;

        auto end = std::chrono::steady_clock::now();
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        totalTime += seconds;
        uint64_t nps = nodes*1000 / std::max<int64_t>(seconds, 1);
        total_nodes += nodes; 

        // Stop early if a forced mate is found
        if (score > 30000 || score < -30000) {
            int n = (score > 0) ? (MATE - score + 1) / 2     // we mate the opponent in n moves
                    : -(MATE + score) / 2;       // we get mated in n moves
            std::println("info depth {} score mate {} nodes {} nps {} time {} pv {}", depth, n, total_nodes, nps, totalTime, moveToString(bestmove)); std::fflush(stdout);
            break;
        }

        // Change to total_nodes later (its at nodes now for fair comparisons as previous versions of engines have that)
        std::println("info depth {} score cp {} nodes {} nps {} time {} pv {}", depth, score, total_nodes, nps, totalTime, moveToString(bestmove)); std::fflush(stdout);

        if (totalTime > soft_limit_ms) break;
    }

    return bestmove;
}
