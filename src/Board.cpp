#include "Board.h"

void Zobrist::init() {
    // Deterministic Mersenne Twister seed ensures identical hash values across runs
    std::mt19937_64 rng(1070372); 

    for (int p = 0; p < 6; ++p) {
        for (int c = 0; c < 2; ++c) {
            for (int sq = 0; sq < 64; ++sq) {
                piece_keys[p][c][sq] = rng();
            }
        }
    }
    side_key = rng();
    for (int i = 0; i < 16; ++i) castle_keys[i] = rng();
    for (int i = 0; i < 64; ++i) ep_keys[i] = rng();
}

void Board::parseFEN(std::string_view fen) {
    // Dissecting FEN

    // Helper lambda to consume and return the next space-delimited token
    auto next_token = [](std::string_view& sv) -> std::string_view {
        auto pos = sv.find(' ');
        std::string_view token = sv.substr(0, pos);
        sv = (pos == std::string_view::npos) ? std::string_view{} : sv.substr(pos + 1);
        return token;
    };

    std::string_view remaining{fen};

    // 1. Piece placement
    std::string_view pos = next_token(remaining);

    // 2. Active color
    std::string_view turn_sv = next_token(remaining);
    turn = (!turn_sv.empty() && turn_sv[0] == 'w') ? WHITE : BLACK;
    oppSide = getOppositeColor(turn);
    if (turn == BLACK) hash ^= zobrist_keys.side_key;

    // 3. Castling rights
    std::string_view cr = next_token(remaining);
    castling_rights = 0;
    if (cr != "-") {
        for (char c : cr) {
            switch (c) {
                case 'K': castling_rights |= (1 << 0); break;
                case 'Q': castling_rights |= (1 << 1); break;
                case 'k': castling_rights |= (1 << 2); break;
                case 'q': castling_rights |= (1 << 3); break;
            }
        }
    }
    hash ^= zobrist_keys.castle_keys[castling_rights];

    // 4. En passant target square
    std::string_view ep = next_token(remaining);
    if (ep != "-" && !ep.empty()) {
        en_passant_sq = getSquareFromNotation(ep);
        hash ^= zobrist_keys.ep_keys[en_passant_sq];
    }

    // 5. Halfmove clock
    std::string_view halfmove_sv = next_token(remaining);
    if (!halfmove_sv.empty()) {
        std::from_chars(halfmove_sv.data(), halfmove_sv.data() + halfmove_sv.size(), halfmove_clock);
    }

    // 6. Fullmove number
    std::string_view fullmove_sv = next_token(remaining);
    if (!fullmove_sv.empty()) {
        std::from_chars(fullmove_sv.data(), fullmove_sv.data() + fullmove_sv.size(), fullmove_no);
    }

    // Parse pos
    int rank = 7; int file = 0;
    for (char c: pos) {
        if (c == '/') {
            rank--; file = 0;
            continue;
        } else if (c >= '1' && c <= '8') {
            for (int i = 0; i < static_cast<int>(c - '0'); i++) {
                grid[(rank*8)+file] = NO_PIECE;
                file++;
            }
        } else {
            int sq = (rank * 8) + file;
            PieceType type = char_to_piece(c);
            U64 occupied_bit = (1ULL << sq);

            occupancy[BOTH] |= occupied_bit; // Filling in both occupancy bitboard
            if (static_cast<int>(c) > 97) {
                // black piece
                occupancy[BLACK] |= occupied_bit; // Occupancy board
                piece_type_bb[BLACK][type] |= occupied_bit; // Piece bitboard
                hash ^= zobrist_keys.piece_keys[type][BLACK][sq]; // Zobrist hash
            } else {
                // white piece
                occupancy[WHITE] |= occupied_bit; // Occupancy board
                piece_type_bb[WHITE][type] |= occupied_bit; // Piece bitboard
                hash ^= zobrist_keys.piece_keys[type][WHITE][sq]; // Zobrist hash
            }

            grid[sq] = static_cast<Piece>(c);

            file++;
        }
    }
}

// For debugging
uint64_t Board::compute_hash_from_scratch() {
    uint64_t computed_hash = 0ULL;
    computed_hash ^= zobrist_keys.castle_keys[castling_rights];
    computed_hash ^= zobrist_keys.ep_keys[en_passant_sq];
    if (turn == BLACK) computed_hash ^= zobrist_keys.side_key;

    for (int piece_type = 0; piece_type < 6; piece_type++) {
        for (int side = 0; side < 2; side++) {
            U64 pieces = piece_type_bb[side][piece_type];
            while (pieces) {
                Square from = static_cast<Square>(std::countr_zero(pieces));
                computed_hash ^= zobrist_keys.piece_keys[piece_type][side][from];
            }
        }
    }

    return computed_hash;
}

void Board::reset() {
    /* Have to reset all of these
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

    uint8_t halfmove_clock;
    uint16_t fullmove_no;

    Piece captured_piece{NO_PIECE};

    std::array<StateHistory, 256> history;
    uint16_t ply_counter{0};
    */
    piece_type_bb = {}; 
    occupancy = {};
    grid = {};
    hash = 0;
    // Turn, opposite side are set in the parseFEN function so no need to be reset
    halfmove_clock = 0;
    fullmove_no = 1;
    castling_rights = 0xF;
    en_passant_sq = SQ_NONE;
    captured_piece = NO_PIECE;
    history = {};
    ply_counter = 0;
}

bool Board::is_square_attacked(Square sq) {
    if (PAWN_ATTACKS[turn][sq] & piece_type_bb[oppSide][PAWN]) return true; // Check with your attack tables
    if (KNIGHT_ATTACKS[sq] & piece_type_bb[oppSide][KNIGHT]) return true;
    if (get_bishop_attacks(sq, occupancy[BOTH]) & (piece_type_bb[oppSide][BISHOP] | piece_type_bb[oppSide][QUEEN])) return true;
    if (get_rook_attacks(sq, occupancy[BOTH]) & (piece_type_bb[oppSide][ROOK] | piece_type_bb[oppSide][QUEEN])) return true;
    if (KING_ATTACKS[sq] & piece_type_bb[oppSide][KING]) return true;

    return false;
}

bool Board::is_king_capturable() { // Move legality check
    Square sq = static_cast<Square>(std::countr_zero(piece_type_bb[oppSide][KING]));

    if (PAWN_ATTACKS[oppSide][sq] & piece_type_bb[turn][PAWN]) return true; // Check from your own side
    if (KNIGHT_ATTACKS[sq] & piece_type_bb[turn][KNIGHT]) return true;
    if (get_bishop_attacks(sq, occupancy[BOTH]) & (piece_type_bb[turn][BISHOP] | piece_type_bb[turn][QUEEN])) return true;
    if (get_rook_attacks(sq, occupancy[BOTH]) & (piece_type_bb[turn][ROOK] | piece_type_bb[turn][QUEEN])) return true;
    if (KING_ATTACKS[sq] & piece_type_bb[turn][KING]) return true;

    return false;
}

void Board::generate_pseudo_legal_moves(MoveList& move_list, bool capturesOnly) {

    get_pawn_moves(move_list, capturesOnly);
    get_piece_moves(move_list, KNIGHT, capturesOnly);
    get_piece_moves(move_list, BISHOP, capturesOnly);
    get_piece_moves(move_list, ROOK, capturesOnly);
    get_piece_moves(move_list, QUEEN, capturesOnly);
    get_piece_moves(move_list, KING, capturesOnly);

    if (!capturesOnly) {
        // Castling
        if (turn == WHITE) {
            // O-O
            if (castling_rights & WHITE_OO) { // Castling rights check
                if (check_castle(oo_castling_paths[WHITE], oo_castling_attack_checks[WHITE])) move_list.push(makeMoveU16(E1, G1, K_CASTLE));
            }

            // O-O-O
            if (castling_rights & WHITE_OOO) { // Castling rights check
                if (check_castle(ooo_castling_paths[WHITE], ooo_castling_attack_checks[WHITE])) move_list.push(makeMoveU16(E1, C1, Q_CASTLE));
            }
        } else {
            // Castling rights check
            // O-O
            if (castling_rights & BLACK_OO) { // Castling rights check
                if (check_castle(oo_castling_paths[BLACK], oo_castling_attack_checks[BLACK])) move_list.push(makeMoveU16(E8, G8, K_CASTLE));
            }

            // O-O-O
            if (castling_rights & BLACK_OOO) { // Castling rights check
                if (check_castle(ooo_castling_paths[BLACK], ooo_castling_attack_checks[BLACK])) move_list.push(makeMoveU16(E8, C8, Q_CASTLE));
            }
        }
    }
}

// int Board::see(Square sq) {
//     int value = 0;

//     // Getting smallest value attacker
//     PieceType piece = PIECE_TYPE_NA;
//     U64 from = 0ULL;
//     if (PAWN_ATTACKS[oppSide][sq] & piece_type_bb[turn][PAWN]) {
//         piece = PAWN; 
//         from = PAWN_ATTACKS[oppSide][sq] & piece_type_bb[turn][PAWN];
//     }
//     else if (KNIGHT_ATTACKS[sq] & piece_type_bb[turn][KNIGHT]) {
//         piece = KNIGHT;
//         from = KNIGHT_ATTACKS[sq] & piece_type_bb[turn][KNIGHT];
//     }
//     else if (get_bishop_attacks(sq, occupancy[BOTH]) & piece_type_bb[turn][BISHOP]) {
//         piece = BISHOP;
//         from = get_bishop_attacks(sq, occupancy[BOTH]) & piece_type_bb[turn][BISHOP];
//     }
//     else if (get_rook_attacks(sq, occupancy[BOTH]) & piece_type_bb[turn][ROOK]) {
//         piece = ROOK;
//         from = get_rook_attacks(sq, occupancy[BOTH]) & piece_type_bb[turn][ROOK];
//     }
//     else if ((get_bishop_attacks(sq, occupancy[BOTH]) | get_rook_attacks(sq, occupancy[BOTH])) & piece_type_bb[turn][QUEEN]) {
//         piece = QUEEN;
//         from = (get_bishop_attacks(sq, occupancy[BOTH]) | get_rook_attacks(sq, occupancy[BOTH])) & piece_type_bb[turn][QUEEN];
//     }
//     else if (KING_ATTACKS[sq] & piece_type_bb[turn][KING]) {
//         piece = KING;
//         from = KING_ATTACKS[sq] & piece_type_bb[turn][KING];
//     }
//     Square from_sq = static_cast<Square>(std::countr_zero(from)); // In case there are multiple pieces of the same type attacking the square, we only pick one

//     if (piece != PIECE_TYPE_NA) {
//         // Making pseudo capture
//         Piece captured_piece = grid[sq];
//         PieceType captured_piece_type = char_to_piece(captured_piece);
//         grid[sq] = grid[from_sq];
//         grid[from_sq] = NO_PIECE;
//         U64 move_mask = (1ULL << from_sq) | (1ULL << sq);
//         piece_type_bb[turn][piece] ^= move_mask;
//         clear_bit(piece_type_bb[oppSide][captured_piece_type], sq);
//         occupancy[turn] ^= move_mask;
//         clear_bit(occupancy[oppSide], sq);
//         clear_bit(occupancy[BOTH], from_sq);
//         std::swap(turn, oppSide);

//         value = std::max(0, see_values[captured_piece_type] - see(sq));
        
//         // std::println("Smallest value attacker: {}", static_cast<int>(piece));
//         // std::println("From: {}", getStringFromSquare(from_sq));
//         // std::println("Capturing: {}", static_cast<char>(captured_piece));
//         // std::println("Current value: {}", value);
//         // std::println();

//         // Undoing pseudo capture
//         std::swap(turn, oppSide);
//         grid[from_sq] = grid[sq];
//         grid[sq] = captured_piece;
//         piece_type_bb[turn][piece] ^= move_mask;
//         set_bit(piece_type_bb[oppSide][captured_piece_type], sq);
//         occupancy[turn] ^= move_mask;
//         set_bit(occupancy[oppSide], sq);
//         set_bit(occupancy[BOTH], from_sq);
//     }

//     return value;
// }

U64 Board::attackers_to(Square to, U64 occ) {
    U64 attackers = 0ULL;
    attackers |= (PAWN_ATTACKS[BLACK][to] & piece_type_bb[WHITE][PAWN])
                | (PAWN_ATTACKS[WHITE][to] & piece_type_bb[BLACK][PAWN]);
    attackers |= KNIGHT_ATTACKS[to] & (piece_type_bb[WHITE][KNIGHT] | piece_type_bb[BLACK][KNIGHT]);
    attackers |= get_bishop_attacks(to, occ) & (piece_type_bb[WHITE][BISHOP] | piece_type_bb[BLACK][BISHOP]);
    attackers |= get_rook_attacks(to, occ) & (piece_type_bb[WHITE][ROOK] | piece_type_bb[BLACK][ROOK]);
    attackers |= (get_bishop_attacks(to, occ) | get_rook_attacks(to, occ)) & (piece_type_bb[WHITE][QUEEN] & piece_type_bb[BLACK][QUEEN]);
    
    return attackers;
}

bool Board::see_ge(Move m, int threshold) {
    int flag = m >> 12;
    if (flag >= 8 || flag == EP_CAPTURE || flag == K_CASTLE || flag == Q_CASTLE) return true; // promo, e.p., castling: treat as fine
    Square from = static_cast<Square>(m & 0x3F), to = static_cast<Square>((m >> 6) & 0x3F);

    int swap = see_values[char_to_piece(grid[to])] - threshold;
    if (swap < 0) return false;
    swap = see_values[char_to_piece(grid[from])] - swap;
    if (swap <= 0) return true;

    U64 occ = occupancy[BOTH] ^ (1ULL << from) ^ (1ULL << to);
    U64 attackers = attackers_to(to, occ);
    U64 bishops_queens = piece_type_bb[WHITE][BISHOP] | piece_type_bb[BLACK][BISHOP]
                       | piece_type_bb[WHITE][QUEEN]  | piece_type_bb[BLACK][QUEEN];
    U64 rooks_queens   = piece_type_bb[WHITE][ROOK]   | piece_type_bb[BLACK][ROOK]
                       | piece_type_bb[WHITE][QUEEN]  | piece_type_bb[BLACK][QUEEN];
    Color stm = turn;
    int res = 1;

    while (true) {
        stm = (stm == WHITE) ? BLACK : WHITE;
        attackers &= occ;
        U64 mine = attackers & occupancy[stm];
        if (!mine) break;
        res ^= 1;
        U64 bb;
        if      ((bb = mine & piece_type_bb[stm][PAWN]))   { if ((swap = see_values[PAWN]   - swap) < res) break; occ ^= bb & -bb; attackers |= get_bishop_attacks(to, occ) & bishops_queens; }
        else if ((bb = mine & piece_type_bb[stm][KNIGHT])) { if ((swap = see_values[KNIGHT] - swap) < res) break; occ ^= bb & -bb; }
        else if ((bb = mine & piece_type_bb[stm][BISHOP])) { if ((swap = see_values[BISHOP] - swap) < res) break; occ ^= bb & -bb; attackers |= get_bishop_attacks(to, occ) & bishops_queens; }
        else if ((bb = mine & piece_type_bb[stm][ROOK]))   { if ((swap = see_values[ROOK]   - swap) < res) break; occ ^= bb & -bb; attackers |= get_rook_attacks(to, occ) & rooks_queens; }
        else if ((bb = mine & piece_type_bb[stm][QUEEN]))  { if ((swap = see_values[QUEEN]  - swap) < res) break; occ ^= bb & -bb;
                                                              attackers |= (get_bishop_attacks(to, occ) & bishops_queens) | (get_rook_attacks(to, occ) & rooks_queens); }
        else return (attackers & ~occupancy[stm]) ? (res ^ 1) : res; // king: only if no enemy attackers remain
    }
    return res;
}

void Board::score_moves(MoveList &move_list, Move tt_move, int search_ply) {
    // Scoring moves for MVV-LVA ordering
    for (size_t i = 0; i < move_list.size(); ++i) {
        Move move = move_list[i];

        // Order: TT_move(2'000'000) -> Q_PROM_CAPTURE(1'200'000 + mvv_lva) -> Q_PROM(1'200'000) -> CAPTURE_BASE(1'000'000 + mvv_lva) 
        //        -> KILLER_1(900'000) -> KILLER_2(800'000) -> Rest of quiet moves(using history score (-16384, 16384(MAX_HISTORY)))
        //        -> Bad captures (-1'000'000) -> Knight underpromotion(-1'500'000) -> Bishop/rook underpromotion(-2'000'000)

        // Capture: higher victim first, then lower attacker first
        // score = CAPTURE_BASE + captured_type * 8 + (5 - moving_piece);

        if (move == tt_move) {
            move_list.scores[i] = 2'000'000; // Highest score for TT move
            continue;
        }

        uint8_t flag = move >> 12;
        PieceType moving_piece = char_to_piece(grid[move & 0x3F]);

        if (flag & 0x4) { // Capture bit
            PieceType captured_type;
            if (flag == EP_CAPTURE) { // En passant
                captured_type = PAWN;
            } else {
                captured_type = char_to_piece(grid[(move >> 6) & 0x3F]); 
            }

            int mvv_lva = captured_type * 8 + (5 - moving_piece);

            if (flag == Q_PROM_CAPTURE) { // Queen capture promotion
                move_list.scores[i] = 1'200'000 + mvv_lva;
                continue;
            }

            if (flag >= 12) { // Underpromotion captures
                if (flag == K_PROM_CAPTURE) move_list.scores[i] = -1'500'000;
                else move_list.scores[i] = -2'000'000;
                continue;
            }

            // Has to be CAPTURE or EP_CAPTURE now
            move_list.scores[i] = 1'000'000 + mvv_lva;
            // if ((see_values[captured_type] >= see_values[moving_piece]) || see_ge(move, 0)) 
            //     move_list.scores[i] = 1'000'000 + mvv_lva; // above killers
            // else 
            //     move_list.scores[i] = -1'000'000 + mvv_lva; // bad captures: after all quiets
            //     // move_list.scores[i] = 700'000 + mvv_lva; // bad captures: after killers

        } else if (flag >= 8) { // Can only be regular promotions(8-11) and not capture promotions    
            if (flag == Q_PROM) move_list.scores[i] = 1'200'000;
            else if (flag == K_PROM) move_list.scores[i] = -1'500'000;
            else move_list.scores[i] = -2'000'000;
        
        } else { // Quiet moves
            if (move == killer_moves[0][search_ply]) move_list.scores[i] = 900'000;
            else if (move == killer_moves[1][search_ply]) move_list.scores[i] = 800'000;
            else move_list.scores[i] = history_moves[turn][moving_piece][(move >> 6) & 0x3F]; // (move >> 6) & 0x3F is the target square
        }
    }
}

bool Board::check_castle(U64 occupancy_path, U64 relevant_squares) {
    if ((occupancy_path & occupancy[BOTH]) == 0) { // Check if path is clear
        while (relevant_squares) {
            Square sq = static_cast<Square>(std::countr_zero(relevant_squares));

            if (is_square_attacked(sq)) return false;

            relevant_squares &= (relevant_squares - 1);
        }
        return true;
    } else {
        return false;
    }
}

void Board::get_piece_moves(MoveList &move_list, PieceType piece, bool capturesOnly) {
    U64 pieces = piece_type_bb[turn][piece];
    while (pieces) {
        Square from = static_cast<Square>(std::countr_zero(pieces));

        U64 attacks = 0;
        if (piece == BISHOP) {
            attacks = get_bishop_attacks(from, occupancy[BOTH]);
        } else if (piece == ROOK) {
            attacks = get_rook_attacks(from, occupancy[BOTH]);
        } else if (piece == QUEEN) {
            attacks = get_queen_attacks(from, occupancy[BOTH]);
        } else if (piece == KNIGHT) {
            attacks = KNIGHT_ATTACKS[from];
        } else if (piece == KING) {
            attacks = KING_ATTACKS[from];
        }
        
        if (capturesOnly) attacks &= occupancy[oppSide];

        while (attacks) {
            Square to = static_cast<Square>(std::countr_zero(attacks));

            if (!get_bit(occupancy[BOTH], to)) { // Empty square
                move_list.push(makeMoveU16(from, to, QUIET_MOVE));
            } else {
                if (get_bit(occupancy[oppSide], to)) { // Capture
                    move_list.push(makeMoveU16(from, to, CAPTURE));
                }
            }

            attacks &= (attacks - 1);
        }

        pieces &= (pieces - 1);
    }
}

void Board::get_pawn_moves(MoveList &move_list, bool capturesOnly) {
    // En passant check
    if (en_passant_sq != SQ_NONE) {
        U64 attackers = PAWN_ATTACKS[oppSide][en_passant_sq] & piece_type_bb[turn][PAWN];

        while (attackers) {
            Square from = static_cast<Square>(std::countr_zero(attackers));
            move_list.push(makeMoveU16(from, en_passant_sq, EP_CAPTURE));
            attackers &= (attackers - 1);
        }
    }

    U64 pawns = piece_type_bb[turn][PAWN];
    while (pawns) {
        Square from = static_cast<Square>(std::countr_zero(pawns));

        U64 captures = PAWN_ATTACKS[turn][from] & occupancy[oppSide];
        while (captures) {
            Square to = static_cast<Square>(std::countr_zero(captures));

            if ((1ULL << to) & last_pawn_ranks[turn]) {
                // Capture promotion
                move_list.push(makeMoveU16(from, to, Q_PROM_CAPTURE));
                move_list.push(makeMoveU16(from, to, R_PROM_CAPTURE));
                move_list.push(makeMoveU16(from, to, B_PROM_CAPTURE));
                move_list.push(makeMoveU16(from, to, K_PROM_CAPTURE));
            } else {
                move_list.push(makeMoveU16(from, to, CAPTURE));
            }

            captures &= (captures - 1);
        }
        
        if (!capturesOnly) {
            if (turn == WHITE) {
                // Check single push
                if (!get_bit(occupancy[BOTH], from+8)) {
                    // Check for promotion
                    if ((1ULL << (from + 8)) & last_pawn_ranks[turn]) {
                        move_list.push(makeMoveU16(from, from+8, Q_PROM));
                        move_list.push(makeMoveU16(from, from+8, R_PROM));
                        move_list.push(makeMoveU16(from, from+8, B_PROM));
                        move_list.push(makeMoveU16(from, from+8, K_PROM));
                    } else {
                        move_list.push(makeMoveU16(from, from+8, QUIET_MOVE));

                        // Check double push
                        if ((1ULL << from) & starting_pawn_ranks[turn]) {
                            if (!get_bit(occupancy[BOTH], from+16)) {
                                move_list.push(makeMoveU16(from, from+16, DOUBLE_PAWN_PUSH));
                            }
                        }
                    }
                }
            } else {
                // Check single push
                if (!get_bit(occupancy[BOTH], from-8)) {
                    // Check for promotion
                    if ((1ULL << (from - 8)) & last_pawn_ranks[turn]) {
                        move_list.push(makeMoveU16(from, from-8, Q_PROM));
                        move_list.push(makeMoveU16(from, from-8, R_PROM));
                        move_list.push(makeMoveU16(from, from-8, B_PROM));
                        move_list.push(makeMoveU16(from, from-8, K_PROM));
                    } else {
                        move_list.push(makeMoveU16(from, from-8, QUIET_MOVE));


                        // Check double push
                        if ((1ULL << from) & starting_pawn_ranks[turn]) {
                            if (!get_bit(occupancy[BOTH], from-16)) {
                                move_list.push(makeMoveU16(from, from-16, DOUBLE_PAWN_PUSH));
                            }
                        }
                    }
                }
            }
        }

        pawns &= (pawns - 1);
    }
}

void Board::make_move(Move move) {
    // Have to update all of these
    /*
    std::array<std::array<U64, 6>, 2> piece_type_bb{};
    std::array<U64, 3> occupancy{}; // WHITE, BLACK, BOTH

    // Mailbox array for O(1) piece lookup on a specific square
    std::array<Piece, 64> grid{};
    
    Color turn{};
    Color oppSide{};

    uint8_t castling_rights{0xF}; // 4-bit mask for KQkq
    Square en_passant_sq{SQ_NONE};

    Piece captured_piece{NO_PIECE};

    uint8_t halfmove_clock;
    uint16_t fullmove_no;

    std::array<StateHistory, 256> history;
    uint16_t ply_counter{0};
    */

    Square to = static_cast<Square>((move >> 6) & 0x3F);
    Square from = static_cast<Square>(move & 0x3F);
    PieceType piece = char_to_piece(grid[from]);
    PieceType captured = char_to_piece(grid[to]); // Could be PIECE_TYPE_NA
    U64 move_mask = (1ULL << from) | (1ULL << to);
    MoveFlag flag = static_cast<MoveFlag>(move >> 12);


    history[ply_counter++] = {castling_rights, en_passant_sq, halfmove_clock, captured_piece, hash};

    hash ^= zobrist_keys.castle_keys[castling_rights]; // Undoing castling rights in hash
    castling_rights &= castling_update_mask[from] & castling_update_mask[to]; // Updating castling rights
    hash ^= zobrist_keys.castle_keys[castling_rights]; // Redoing new castling rights in hash
    
    if (en_passant_sq != SQ_NONE) hash ^= zobrist_keys.ep_keys[en_passant_sq]; // Undoing en passant square in hash
    en_passant_sq = SQ_NONE; // Always reset to SQ_NONE unless double pawn push

    captured_piece = grid[to];

    if (piece == PAWN) halfmove_clock = 0; // Resets at pawn move or capture
    else halfmove_clock++;
    
    if (turn == BLACK) fullmove_no++;

    occupancy[turn] ^= move_mask;

    // Hash updates
    hash ^= zobrist_keys.piece_keys[piece][turn][from];
    hash ^= zobrist_keys.piece_keys[piece][turn][to];
    hash ^= zobrist_keys.side_key;

    if (flag < 8) {
        // QUIET_MOVE
        occupancy[BOTH] ^= move_mask;
        piece_type_bb[turn][piece] ^= move_mask;
        grid[to] = grid[from];
        grid[from] = NO_PIECE;

        if (flag == DOUBLE_PAWN_PUSH) {
            en_passant_sq = turn == WHITE ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
            hash ^= zobrist_keys.ep_keys[en_passant_sq];
        } else if ((flag == K_CASTLE) || (flag == Q_CASTLE)) {  // Castling
            Square rf, rt;
            if (turn == WHITE) {
                if (flag == K_CASTLE) {
                    rf = H1; rt = F1;
                } else {
                    rf = A1; rt = D1;
                }
            } else {
                if (flag == K_CASTLE) {
                    rf = H8; rt = F8;
                } else {
                    rf = A8; rt = D8;
                }
            }
            U64 rook_move = (1ULL << rf) | (1ULL << rt);

            piece_type_bb[turn][ROOK] ^= rook_move;
            occupancy[turn] ^= rook_move;
            occupancy[BOTH] ^= rook_move;
            grid[rt] = grid[rf]; grid[rf] = NO_PIECE;
        } else if (flag > 3) { // Capture and En passant
            set_bit(occupancy[BOTH], to); // Because previous XOR relied on dest square being empty

            if (flag == CAPTURE) { // Regular capture
                clear_bit(piece_type_bb[oppSide][captured], to);
                clear_bit(occupancy[oppSide], to);
                hash ^= zobrist_keys.piece_keys[captured][oppSide][to];
            } else { // En passant
                if (turn == WHITE) {
                    clear_bit(piece_type_bb[oppSide][PAWN], to - 8);
                    clear_bit(occupancy[oppSide], to - 8);
                    clear_bit(occupancy[BOTH], to - 8);
                    captured_piece = grid[to - 8];
                    grid[to - 8] = NO_PIECE;
                    hash ^= zobrist_keys.piece_keys[PAWN][oppSide][to - 8];
                } else {
                    clear_bit(piece_type_bb[oppSide][PAWN], to + 8);
                    clear_bit(occupancy[oppSide], to + 8);
                    clear_bit(occupancy[BOTH], to + 8);
                    captured_piece = grid[to + 8];
                    grid[to + 8] = NO_PIECE;
                    hash ^= zobrist_keys.piece_keys[PAWN][oppSide][to + 8];
                }
            }
            
            halfmove_clock = 0;
        }
    } else { // Promotions and Capture promotions
        hash ^= zobrist_keys.piece_keys[piece][turn][to]; // Undo moving original piece to destination square
        clear_bit(piece_type_bb[turn][piece], from);
        grid[from] = NO_PIECE;
        
        if (flag < 12) { // Regular promotions
            occupancy[BOTH] ^= move_mask;

        } else { // Capture promotions
            clear_bit(occupancy[BOTH], from);
            clear_bit(occupancy[oppSide], to);
            clear_bit(piece_type_bb[oppSide][captured], to);
            hash ^= zobrist_keys.piece_keys[captured][oppSide][to];
        }

        if ((flag == Q_PROM) || (flag == Q_PROM_CAPTURE)) {
            set_bit(piece_type_bb[turn][QUEEN], to); hash ^= zobrist_keys.piece_keys[QUEEN][turn][to];
            grid[to] = turn == WHITE ? W_QUEEN : B_QUEEN;
        } else if ((flag == R_PROM) || (flag == R_PROM_CAPTURE)) {
            set_bit(piece_type_bb[turn][ROOK], to); hash ^= zobrist_keys.piece_keys[ROOK][turn][to];
            grid[to] = turn == WHITE ? W_ROOK : B_ROOK;
        } else if ((flag == B_PROM) || (flag == B_PROM_CAPTURE)) {
            set_bit(piece_type_bb[turn][BISHOP], to); hash ^= zobrist_keys.piece_keys[BISHOP][turn][to];
            grid[to] = turn == WHITE ? W_BISHOP : B_BISHOP;
        } else if ((flag == K_PROM) || (flag == K_PROM_CAPTURE)) {
            set_bit(piece_type_bb[turn][KNIGHT], to); hash ^= zobrist_keys.piece_keys[KNIGHT][turn][to];
            grid[to] = turn == WHITE ? W_KNIGHT: B_KNIGHT;
        }
    }

    std::swap(turn, oppSide);

    // after make_move, in debug builds only
    // assert(hash == compute_hash_from_scratch());
}

void Board::unmake_move(Move move) {
    // Can only be called with move that was just made

    Square to = static_cast<Square>((move >> 6) & 0x3F);
    Square from = static_cast<Square>(move & 0x3F);
    PieceType piece = char_to_piece(grid[to]);
    PieceType captured = char_to_piece(captured_piece); // could be na
    U64 move_mask = (1ULL << from) | (1ULL << to);
    MoveFlag flag = static_cast<MoveFlag>(move >> 12);

    std::swap(turn, oppSide);

    if (turn == BLACK) fullmove_no--;

    occupancy[turn] ^= move_mask; // Will reverse move (if it was just made)

    if (flag < 8) {
        // QUIET_MOVE
        occupancy[BOTH] ^= move_mask;
        piece_type_bb[turn][piece] ^= move_mask;
        grid[from] = grid[to];
        grid[to] = captured_piece;

        if ((flag == K_CASTLE) || (flag == Q_CASTLE)) {  // Undo rook move in castling
            Square rf, rt;
            if (turn == WHITE) {
                if (flag == K_CASTLE) {
                    rf = H1; rt = F1;
                } else {
                    rf = A1; rt = D1;
                }
            } else {
                if (flag == K_CASTLE) {
                    rf = H8; rt = F8;
                } else {
                    rf = A8; rt = D8;
                }
            }
            U64 rook_move = (1ULL << rf) | (1ULL << rt);

            piece_type_bb[turn][ROOK] ^= rook_move;
            occupancy[turn] ^= rook_move;
            occupancy[BOTH] ^= rook_move;
            grid[rf] = grid[rt]; grid[rt] = NO_PIECE;

        } else if (flag > 3) { // Capture and En passant

            if (flag == CAPTURE) { // Regular capture
                set_bit(piece_type_bb[oppSide][captured], to);
                set_bit(occupancy[oppSide], to);
                set_bit(occupancy[BOTH], to);
            } else { // En passant
                if (turn == WHITE) {
                    set_bit(piece_type_bb[oppSide][PAWN], to - 8);
                    set_bit(occupancy[oppSide], to - 8);
                    set_bit(occupancy[BOTH], to - 8);
                    grid[to] = NO_PIECE;
                    grid[to - 8] = captured_piece;
                } else {
                    set_bit(piece_type_bb[oppSide][PAWN], to + 8);
                    set_bit(occupancy[oppSide], to + 8);
                    set_bit(occupancy[BOTH], to + 8);
                    grid[to] = NO_PIECE;
                    grid[to + 8] = captured_piece;
                }
            }
        }
    } else { // Promotions and Capture promotions
        set_bit(piece_type_bb[turn][PAWN], from);
        grid[from] = turn == WHITE ? W_PAWN : B_PAWN;
        
        
        if (flag < 12) { // Regular promotions
            occupancy[BOTH] ^= move_mask;
            grid[to] = NO_PIECE;

        } else { // Capture promotions
            set_bit(occupancy[BOTH], from);
            set_bit(occupancy[oppSide], to);
            set_bit(piece_type_bb[oppSide][captured], to);
            grid[to] = captured_piece;
        }

        if ((flag == Q_PROM) || (flag == Q_PROM_CAPTURE)) {
            clear_bit(piece_type_bb[turn][QUEEN], to); hash ^= zobrist_keys.piece_keys[QUEEN][turn][to];
        } else if ((flag == R_PROM) || (flag == R_PROM_CAPTURE)) {
            clear_bit(piece_type_bb[turn][ROOK], to); hash ^= zobrist_keys.piece_keys[ROOK][turn][to];
        } else if ((flag == B_PROM) || (flag == B_PROM_CAPTURE)) {
            clear_bit(piece_type_bb[turn][BISHOP], to); hash ^= zobrist_keys.piece_keys[BISHOP][turn][to];
        } else if ((flag == K_PROM) || (flag == K_PROM_CAPTURE)) {
            clear_bit(piece_type_bb[turn][KNIGHT], to); hash ^= zobrist_keys.piece_keys[KNIGHT][turn][to];
        }
    }

    ply_counter--;
    castling_rights = history[ply_counter].castling_rights;
    en_passant_sq = history[ply_counter].en_passant_sq;
    halfmove_clock = history[ply_counter].halfmove_clock;
    captured_piece = history[ply_counter].captured_piece;
    hash = history[ply_counter].hash;
}

void Board::make_null_move() {
    history[ply_counter++] = {castling_rights, en_passant_sq, halfmove_clock, captured_piece, hash};

    if (en_passant_sq != SQ_NONE) hash ^= zobrist_keys.ep_keys[en_passant_sq]; // Undoing en passant square in hash
    en_passant_sq = SQ_NONE; // Always reset to SQ_NONE unless double pawn push

    std::swap(turn, oppSide);
    hash ^= zobrist_keys.side_key;

    halfmove_clock++;
}

void Board::unmake_null_move() {
    std::swap(turn, oppSide);
    hash ^= zobrist_keys.side_key;

    ply_counter--;
    castling_rights = history[ply_counter].castling_rights;
    en_passant_sq = history[ply_counter].en_passant_sq;
    halfmove_clock = history[ply_counter].halfmove_clock;
    captured_piece = history[ply_counter].captured_piece;
    hash = history[ply_counter].hash;
}

// For testing and debugging

void Board::printAllBoards() {
    std::println("Side to move: {}", turn == WHITE ? "WHITE" : "BLACK");

    printBitboard(piece_type_bb[WHITE][QUEEN]);
    printBitboard(piece_type_bb[WHITE][ROOK]);
    printBitboard(piece_type_bb[WHITE][BISHOP]);
    printBitboard(piece_type_bb[WHITE][KNIGHT]);
    printBitboard(piece_type_bb[WHITE][PAWN]);
    printBitboard(piece_type_bb[WHITE][KING]);
    printBitboard(occupancy[WHITE]);

    printBitboard(piece_type_bb[BLACK][QUEEN]);
    printBitboard(piece_type_bb[BLACK][ROOK]);
    printBitboard(piece_type_bb[BLACK][BISHOP]);
    printBitboard(piece_type_bb[BLACK][KNIGHT]);
    printBitboard(piece_type_bb[BLACK][PAWN]);
    printBitboard(piece_type_bb[BLACK][KING]);
    printBitboard(occupancy[BLACK]);

    printBitboard(occupancy[BOTH]);
    std::cout << grid << "\n";
}

bool Board::operator==(const Board& other) const {
    if (turn != other.turn) {
        std::println("Different turns");
        return false;
    }
    if (piece_type_bb[turn][QUEEN] != other.piece_type_bb[turn][QUEEN]) {
        printBitboard(piece_type_bb[turn][QUEEN]);
        printBitboard(other.piece_type_bb[turn][QUEEN]);
        std::println("Different queens");
        return false;
    }
    if (piece_type_bb[turn][ROOK] != other.piece_type_bb[turn][ROOK]) {
        printBitboard(piece_type_bb[turn][ROOK]);
        printBitboard(other.piece_type_bb[turn][ROOK]);
        std::println("Different rooks");
        return false; 
    }
    if (piece_type_bb[turn][BISHOP] != other.piece_type_bb[turn][BISHOP]) {
        printBitboard(piece_type_bb[turn][BISHOP]);
        printBitboard(other.piece_type_bb[turn][BISHOP]);
        std::println("Different bishops");
        return false; 
    }
    if (piece_type_bb[turn][KNIGHT] != other.piece_type_bb[turn][KNIGHT]) {
        printBitboard(piece_type_bb[turn][KNIGHT]);
        printBitboard(other.piece_type_bb[turn][KNIGHT]);
        std::println("Different knights");
        return false;
    }
    if (piece_type_bb[turn][PAWN] != other.piece_type_bb[turn][PAWN]) {
        printBitboard(piece_type_bb[turn][PAWN]);
        printBitboard(other.piece_type_bb[turn][PAWN]);
        std::println("Different pawns");
        return false;
    }
    if (piece_type_bb[turn][KING] != other.piece_type_bb[turn][KING]) {
        printBitboard(piece_type_bb[turn][KING]);
        printBitboard(other.piece_type_bb[turn][KING]);
        std::println("Different kings");
        return false;
    }
    if (occupancy[turn] != other.occupancy[turn]) {
        printBitboard(occupancy[turn]);
        printBitboard(other.occupancy[turn]);
        std::println("Different occupancy");
        return false;
    }
    if (occupancy[oppSide] != other.occupancy[oppSide]) {
        printBitboard(occupancy[oppSide]);
        printBitboard(other.occupancy[oppSide]);
        std::println("Different occupancy");
        return false;
    }
    if (occupancy[BOTH] != other.occupancy[BOTH]) {
        printBitboard(occupancy[BOTH]);
        printBitboard(other.occupancy[BOTH]);
        std::println("Different occupancy");
        return false;
    }
    if (grid != other.grid) {
        std::cout << grid << "\n";
        std::cout << other.grid << "\n";
        std::println("Different grids");
        return false;
    }
    if (hash != other.hash) {
        std::println("Different hashes");
        return false;
    }
    if (castling_rights != other.castling_rights) {
        std::println("Different castling rights");
        return false;
    }
    if (en_passant_sq != other.en_passant_sq) {
        std::println("Different en passant squares");
        return false;
    }
    if (halfmove_clock != other.halfmove_clock) {
        std::println("Different halfmove clocks");
        return false;
    }
    if (fullmove_no != other.fullmove_no) {
        std::println("Different fullmove numbers");
        return false;
    }
    if (captured_piece != other.captured_piece) {
        std::println("Different captured pieces");
        return false;
    }
    return true;
}
