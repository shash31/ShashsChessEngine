#pragma once

#include <algorithm>
#include "utils.h"

// Lookup tables for Zobrist random numbers
struct Zobrist {
    // [PieceType][Color][Square] -> 6 * 2 * 64
    uint64_t piece_keys[6][2][64];
    uint64_t side_key;
    uint64_t castle_keys[16]; // 4-bit bitmask for castling rights
    uint64_t ep_keys[64];     // En passant target square (0-63, or unused if no EP)

    void init();
};

inline Zobrist zobrist_keys;

struct StateHistory { // For things not shown by moves
    uint8_t castling_rights{0xF}; // 4-bit mask for KQkq
    Square en_passant_sq{SQ_NONE};
    uint8_t halfmove_clock;
    Piece captured_piece;
    uint64_t hash;
};

struct MoveList {
    std::array<Move, 256> moves;
    std::array<int, 256> scores;
    int count = 0;

    void push(Move move) {
        moves[count++] = move;
    }

    int size() const { return count; }
    Move& operator[](int index) { return moves[index]; }
    const Move& operator[](int index) const { return moves[index]; }
};

// killer moves [id][ply]
inline std::array<std::array<int, 64>, 2> killer_moves{};

// history moves [sideToMove][piece][to]
inline std::array<std::array<std::array<int, 64>, 6>, 2> history_moves{};
inline constexpr int MAX_HISTORY = 16384;

struct Board {
    // 12 Direct Bitboards (6 piece types * 2 colors)
    // pieces[WHITE][PAWN], pieces[BLACK][KNIGHT], etc.
    std::array<std::array<U64, 6>, 2> piece_type_bb{};
    std::array<U64, 3> occupancy{}; // WHITE, BLACK, BOTH

    // Mailbox array for O(1) piece lookup on a specific square
    std::array<Piece, 64> grid{};

    uint64_t hash{};
    
    Color turn{};
    Color oppSide{};

    uint8_t castling_rights{0xF}; // 4-bit mask for KQkq
    Square en_passant_sq{SQ_NONE};

    uint8_t halfmove_clock{};
    uint8_t fullmove_no{1};

    Piece captured_piece{NO_PIECE};

    std::array<StateHistory, 256> history{};
    uint8_t ply_counter{0};

    void reset();

    void parseFEN(std::string_view fen);

    // For debugging
    uint64_t compute_hash_from_scratch();

    bool is_square_attacked(Square sq);

    bool is_king_capturable();

    void generate_pseudo_legal_moves(MoveList &move_list, bool capturesOnly=false);

    // int see(Square sq);

    U64 attackers_to(Square to, U64 occ);

    bool see_ge(Move m, int threshold);

    void score_moves(MoveList &move_list, Move tt_move, int search_ply);

    bool check_castle(U64 occupancy_path, U64 relevant_squares);

    void get_piece_moves(MoveList &move_list, PieceType piece, bool capturesOnly);

    void get_pawn_moves(MoveList &move_list, bool capturesOnly);

    void make_move(Move move);

    void unmake_move(Move move);

    void make_null_move();

    void unmake_null_move();

    // For testing and debugging

    void printAllBoards();

    bool operator==(const Board& other) const;

};
