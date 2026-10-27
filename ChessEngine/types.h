#pragma once

#include <cstdint>
#include <array>

using U64 = uint64_t;

// Representing moves as 16 bit integer
// Flags(4 bits) - To Square (6 bits) - From Square (6 bits)
// 15..12          11..6                5..0
// Flags:
// 0000	Quiet Move
// 0001	Double Pawn Push
// 0010	Kingside Castle
// 0011	Queenside Castle
// 0100	Standard Capture
// 0101	En Passant Capture
// 1000 to 1011	Knight, Bishop, Rook, Queen Promotion (Quiet)
// 1100 to 1111	Knight, Bishop, Rook, Queen Promotion (Capture)
using Move = uint16_t;

enum MoveFlag : uint8_t {
    QUIET_MOVE = 0,
    DOUBLE_PAWN_PUSH,
    K_CASTLE,
    Q_CASTLE,
    CAPTURE,
    EP_CAPTURE,
    K_PROM = 8,
    B_PROM,
    R_PROM,
    Q_PROM,
    K_PROM_CAPTURE,
    B_PROM_CAPTURE,
    R_PROM_CAPTURE,
    Q_PROM_CAPTURE
};

enum Square : uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE = 64
};

enum PieceType : uint8_t {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    PIECE_TYPE_NA = 6
};

enum Piece : uint8_t {
    NO_PIECE=' ', W_PAWN='P', W_KNIGHT='N', W_BISHOP='B', W_ROOK='R', W_QUEEN='Q', W_KING='K',
    B_PAWN='p', B_KNIGHT='n', B_BISHOP='b', B_ROOK='r', B_QUEEN='q', B_KING='k'
};

enum Color : uint8_t { WHITE=0, BLACK, BOTH };

enum CastlingRights : uint8_t {
    NO_CASTLING = 0,
    WHITE_OO    = 1 << 0, // 1 (0b0001)
    WHITE_OOO   = 1 << 1, // 2 (0b0010)
    BLACK_OO    = 1 << 2, // 4 (0b0100)
    BLACK_OOO   = 1 << 3, // 8 (0b1000)
    
    WHITE_CASTLE = WHITE_OO | WHITE_OOO, // 3 (0b0011)
    BLACK_CASTLE = BLACK_OO | BLACK_OOO, // 12 (0b1100)
    ANY_CASTLE   = WHITE_CASTLE | BLACK_CASTLE // 15 (0b1111)
};
