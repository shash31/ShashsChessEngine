#include "eval.h"

// #define PAWN   0
// #define KNIGHT 1
// #define BISHOP 2
// #define ROOK   3
// #define QUEEN  4
// #define KING   5

/* board representation */
// #define WHITE  0
// #define BLACK  1


// void init_psq_tables()
// {
//     int pc, p, sq;
//     for (p = PAWN, pc = WHITE_PAWN; p <= KING; pc += 2, p++) {
//         for (sq = 0; sq < 64; sq++) {
//             // mg_table[pc]  [sq] = mg_value[p] + mg_pesto_table[p][sq];
//             // eg_table[pc]  [sq] = eg_value[p] + eg_pesto_table[p][sq];
//             mg_table[pc]  [sq] = mg_value[p] + mg_pesto_table[p][FLIP(sq)];
//             eg_table[pc]  [sq] = eg_value[p] + eg_pesto_table[p][FLIP(sq)];
//             // mg_table[pc+1][sq] = mg_value[p] + mg_pesto_table[p][FLIP(sq)];
//             // eg_table[pc+1][sq] = eg_value[p] + eg_pesto_table[p][FLIP(sq)];
//             mg_table[pc+1][sq] = mg_value[p] + mg_pesto_table[p][sq];
//             eg_table[pc+1][sq] = eg_value[p] + eg_pesto_table[p][sq];
//         }
//     }
// }

int pesto_eval(Board& board)
{
    int mg[2];
    int eg[2];
    int gamePhase = 0;

    mg[WHITE] = 0;
    mg[BLACK] = 0;
    eg[WHITE] = 0;
    eg[BLACK] = 0;

    /* evaluate each piece */
    U64 occupancy = board.occupancy[BOTH];
    while (occupancy) {
        Square sq = static_cast<Square>(std::countr_zero(occupancy));
        Color color = get_bit(board.occupancy[WHITE], sq) ? WHITE : BLACK;
        PieceType p = char_to_piece(board.grid[sq]);

        mg[color] += color == WHITE ? mg_value[p]+mg_pesto_table[p][FLIP(sq)] : mg_value[p]+mg_pesto_table[p][sq];
        eg[color] += color == WHITE ? eg_value[p]+eg_pesto_table[p][FLIP(sq)] : eg_value[p]+eg_pesto_table[p][sq];

        gamePhase += gamephaseInc[p];

        occupancy &= (occupancy - 1);
    }

    // for (int sq = 0; sq < 64; ++sq) {
    //     int pc = board[sq];
    //     if (pc != EMPTY) {
    //         mg[PCOLOR(pc)] += mg_table[pc][sq];
    //         eg[PCOLOR(pc)] += eg_table[pc][sq];
    //         gamePhase += gamephaseInc[pc];
    //     }
    // }

    /* tapered eval */
    int mgScore = mg[board.turn] - mg[board.oppSide];
    int egScore = eg[board.turn] - eg[board.oppSide];
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; /* in case of early promotion */
    int egPhase = 24 - mgPhase;
    return (mgScore * mgPhase + egScore * egPhase) / 24;
}
