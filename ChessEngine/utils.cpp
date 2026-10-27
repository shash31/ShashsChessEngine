#include "utils.h"

std::ostream& operator<<(std::ostream& os, const std::array<Piece, 64>& grid) {
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            os << static_cast<char>(grid[(i * 8) + j]) << ' ';
        }
        os << '\n';
    }
    return os;
}

PieceType char_to_piece(char p) {
    switch (p) {
        case 'p':
        case 'P':
            return PAWN;
            break;
        case 'n':
        case 'N':
            return KNIGHT;
            break;
        case 'b':
        case 'B':
            return BISHOP;
            break;
        case 'r':
        case 'R':
            return ROOK;
            break;
        case 'q':
        case 'Q':
            return QUEEN;
            break;
        case 'k':
        case 'K':
            return KING;
            break;
        default:
            return PIECE_TYPE_NA;
            break;
    }
}

void printBitboard(U64 board) {
    std::ostringstream strm;
    for (int rank = 7; rank >= 0; rank--) {
        strm << rank+1 << "|  ";

        for (unsigned int file = 0; file < 8; file++) {
            char c = (board & (1ULL << ((rank * 8) + file))) != 0 ? 'X' : '*';
            strm << c << " ";
        }

        strm << '\n';
    }

    strm << "   ________________\n";
    strm << "    A B C D E F G H\n";
    // strm << "    0 1 2 3 4 5 6 7\n";

    strm << board << '\n';

    std::cout << strm.str() << std::endl;
}

void init_tables() {
    init_leaper_attack_tables();
    // init_magic_numbers();
    init_sliding_piece_attack_tables();
    // return true;
}

constexpr U64 bit_if(int target, U64 mask) {
    if (target < 0 || target > 63) return 0ULL;
    U64 b = 1ULL << target;
    return b & mask;
}

constexpr void init_leaper_attack_tables() {
        // Generating attack bitboards

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                int sq = (i*8)+j;
                
                // Pawns
                PAWN_ATTACKS[WHITE][sq] = 0ULL;
                PAWN_ATTACKS[BLACK][sq] = 0ULL;
                PAWN_ATTACKS[WHITE][sq] = bit_if(sq + 9, not_a_file) | bit_if(sq + 7, not_h_file);
                PAWN_ATTACKS[BLACK][sq] = bit_if(sq - 7, not_a_file) | bit_if(sq - 9, not_h_file);

                // Knights
                KNIGHT_ATTACKS[sq] = 0ULL;
                // NNW, NNE, EEN, EES, SSE, SSW, WWS, WWN
                // +15, +17, +10, -6, -15, -17, -10, +6 
                KNIGHT_ATTACKS[sq] =
                    bit_if(sq + 15, not_h_file)  | bit_if(sq + 17, not_a_file)  |
                    bit_if(sq + 10, not_ab_file) | bit_if(sq - 6,  not_ab_file) |
                    bit_if(sq - 15, not_a_file)  | bit_if(sq - 17, not_h_file)  |
                    bit_if(sq - 10, not_gh_file) | bit_if(sq + 6,  not_gh_file);
                // if ((1ULL << (sq+15)) & not_h_file) KNIGHT_ATTACKS[sq] |= 1ULL << (sq + 15);
                // if ((1ULL << (sq+17)) & not_a_file) KNIGHT_ATTACKS[sq] |= 1ULL << (sq + 17);
                // if ((1ULL << (sq+10)) & not_ab_file) KNIGHT_ATTACKS[sq] |= 1ULL << (sq + 10);
                // if (((sq-6) >= 0) && ((1ULL << (sq-6)) & not_ab_file)) KNIGHT_ATTACKS[sq] |= 1ULL << (sq - 6);
                // if (((sq-15) >= 0) && ((1ULL << (sq-15)) & not_a_file)) KNIGHT_ATTACKS[sq] |= 1ULL << (sq - 15);
                // if (((sq-17) >= 0) && ((1ULL << (sq-17)) & not_h_file)) KNIGHT_ATTACKS[sq] |= 1ULL << (sq - 17);
                // if (((sq-10) >= 0) && ((1ULL << (sq-10)) & not_gh_file)) KNIGHT_ATTACKS[sq] |= 1ULL << (sq - 10);
                // if ((1ULL << (sq+6)) & not_gh_file) KNIGHT_ATTACKS[sq] |= 1ULL << (sq + 6);

                // Kings
                KING_ATTACKS[sq] = 0ULL;
                // NW, N, NE, E, SE, S, SW, W
                // +7,+8, +9,+1, -7,-8,-9, -1
                KING_ATTACKS[sq] =
                    bit_if(sq + 7, not_h_file) | bit_if(sq + 8, ~0ULL) | bit_if(sq + 9, not_a_file) |
                    bit_if(sq + 1, not_a_file) | bit_if(sq - 1, not_h_file) |
                    bit_if(sq - 7, not_a_file) | bit_if(sq - 8, ~0ULL) | bit_if(sq - 9, not_h_file);
                // if ((sq+8) < 64) { // North check
                //     if ((1ULL << (sq+7)) & not_h_file) KING_ATTACKS[sq] |= 1ULL << (sq + 7);
                //     KING_ATTACKS[sq] |= 1ULL << (sq + 8);
                //     if ((1ULL << (sq+9)) & not_a_file) KING_ATTACKS[sq] |= 1ULL << (sq + 9);
                // }

                // if ((1ULL << (sq+1)) & not_a_file) KING_ATTACKS[sq] |= 1ULL << (sq + 1); // East
                
                // if ((sq-8) >= 0) { // South check
                //     if ((1ULL << (sq-7)) & not_a_file) KING_ATTACKS[sq] |= 1ULL << (sq - 7);
                //     KING_ATTACKS[sq] |= 1ULL << (sq - 8); 
                //     if ((1ULL << (sq-9)) & not_h_file) KING_ATTACKS[sq] |= 1ULL << (sq - 9);
                // }

                // if ((1ULL << (sq-1)) & not_h_file) KING_ATTACKS[sq] |= 1ULL << (sq - 1); // West

                
                // Attack mask tables for Bishops & Rooks (Queen table is just OR of bishop and rook)
                BISHOP_ATTACK_MASK[sq] = 0ULL;
                ROOK_ATTACK_MASK[sq] = 0ULL;

                int r, f;
                // Diagonals (Bishops)
                // NE Direction
                for (r=i+1, f=j+1; (r<7 && f<7); r++, f++) BISHOP_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                // SE Direction
                for (r=i-1, f=j+1; (r>0 && f<7); r--, f++) BISHOP_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                // SW Direction
                for (r=i-1, f=j-1; (r>0 && f>0); r--, f--) BISHOP_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                // NW Direction
                for (r=i+1, f=j-1; (r<7 && f>0); r++, f--) BISHOP_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                // Files & Ranks (Rooks)
                f = j;
                for (r=i+1; r<7; r++) ROOK_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                for (r=i-1; r>0; r--) ROOK_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                r = i;
                for (f=j+1; f<7; f++) ROOK_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
                for (f=j-1; f>0; f--) ROOK_ATTACK_MASK[sq] |= 1ULL << ((r*8)+f);
            }
        }

        // Testing
        // printBitboard(PAWN_ATTACKS[BLACK][A7]);
        // printBitboard(PAWN_ATTACKS[BLACK][E7]);
        // printBitboard(PAWN_ATTACKS[BLACK][H4]);
        // printBitboard(KNIGHT_ATTACKS[A1]);
        // printBitboard(KNIGHT_ATTACKS[H8]);
        // printBitboard(KNIGHT_ATTACKS[H6]);
        // printBitboard(KING_ATTACKS[D5]);
        // printBitboard(KING_ATTACKS[A8]);
        // printBitboard(KING_ATTACKS[E1]);
        // printBitboard(KING_ATTACKS[H1]);
        // printBitboard(BISHOP_ATTACK_MASK[E4]);
        // printBitboard(BISHOP_ATTACK_MASK[A1]);
        // printBitboard(ROOK_ATTACK_MASK[E1]);
        // printBitboard(ROOK_ATTACK_MASK[H1]);
}
