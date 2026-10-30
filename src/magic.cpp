#include "magic.h"

U64 calc_bishop_attack_ray(int square, U64 blockers) {
    U64 attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;
    int r, f;
    // NE Direction;
    for (r=rank+1,f=file+1;(r<8)&&(f<8);r++,f++) {
        attacks |= 1ULL << ((r*8)+f);
        if ((1ULL<<((r*8)+f)) & blockers) break;
    }
    // SE Direction
    for (r=rank-1,f=file+1;(r>=0)&&(f<8);r--,f++) {
        attacks |= 1ULL << ((r*8)+f);
        if ((1ULL<<((r*8)+f)) & blockers) break;
    }
    // SW Direction
    for (r=rank-1,f=file-1;(r>=0)&&(f>=0);r--,f--) {
        attacks |= 1ULL << ((r*8)+f);
        if ((1ULL<<((r*8)+f)) & blockers) break;
    }
    // NW Direction
    for (r=rank+1,f=file-1;(r<8)&&(f>=0);r++,f--) {
        attacks |= 1ULL << ((r*8)+f);
        if ((1ULL<<((r*8)+f)) & blockers) break;
    }

    return attacks;
}

U64 calc_rook_attack_ray(int square, U64 blockers) {
    U64 attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;
    int r, f;
    // N Direction;
    for (r=rank+1;r<8;r++) {
        attacks |= 1ULL << ((r*8)+file);
        if ((1ULL<<((r*8)+file)) & blockers) break;
    }
    // E Direction
    for (f=file+1;f<8;f++) {
        attacks |= 1ULL << ((rank*8)+f);
        if ((1ULL<<((rank*8)+f)) & blockers) break;
    }
    // S Direction
    for (r=rank-1;r>=0;r--) {
        attacks |= 1ULL << ((r*8)+file);
        if ((1ULL<<((r*8)+file)) & blockers) break;
    }
    // W Direction
    for (f=file-1;f>=0;f--) {
        attacks |= 1ULL << ((rank*8)+f);
        if ((1ULL<<((rank*8)+f)) & blockers) break;
    }

    return attacks;
}

// generate 64-bit pseudo legal numbers
U64 get_random_U64_number() {
    // 1. Obtain a seed from the hardware
    std::random_device rd;

    // 2. Initialize the 64-bit Mersenne Twister engine
    std::mt19937_64 gen(rd());

    // 3. Define the distribution across the full 64-bit unsigned range
    std::uniform_int_distribution<U64> distrib(
        0, 
        std::numeric_limits<U64>::max()
    );

    // 4. Generate the 64-bit number
    U64 random_num = distrib(gen);

    return random_num;
}

// generate magic number candidate
U64 generate_magic_number() {
    return get_random_U64_number() & get_random_U64_number() & get_random_U64_number();
}

U64 test_magic_numbers(int square, bool isBishop) {
    U64 attack_mask = isBishop ? BISHOP_ATTACK_MASK[square] : ROOK_ATTACK_MASK[square];
    int relevant_bits = isBishop ? bishop_relevant_bits[square] : rook_relevant_bits[square];

    int total_patterns = 1 << relevant_bits;
    std::vector<U64> blockers(total_patterns);
    std::vector<U64> attacks(total_patterns);

    std::vector<U64> test_table(total_patterns);

    // Populate all possible blockers and attack arrays
    U64 b = 0ULL;
    int count = 0;
    do {
        blockers[count] = b;
        attacks[count] = isBishop ? calc_bishop_attack_ray(square, blockers[count]) : calc_rook_attack_ray(square, blockers[count]);
        count++;
        b = (b - attack_mask) & attack_mask; // Carry-rippling subset iteration
    } while (b);

    // Testing magic numbers
    while (true) {
        U64 magic_number = generate_magic_number();
        if ((std::popcount(magic_number) * 0xFF00000000000000) < 6) continue;

        bool fail{false};

        std::fill(test_table.begin(), test_table.end(), 0ULL);
        
        for (int i = 0; i < total_patterns; i++) {
            // int index = ((attack_mask & blockers[i]) * magic_number) >> (64 - (relevant_bits - 1));
            int index = ((attack_mask & blockers[i]) * magic_number) >> (64 - relevant_bits);
        
            if (test_table[index] == 0ULL) {
                test_table[index] = attacks[i];
            } else if (test_table[index] != attacks[i]) {
                fail = true;
                break;
            }
        }
        if (!fail) return magic_number;
    }
}

// void init_magic_numbers() {
//     std::println("Finding magic numbers for bishops");
//     for (int i = 0; i < 64; i++) {
//         BISHOP_MAGIC_NUMS[i] = test_magic_numbers(i, true);
//         std::println("{}", BISHOP_MAGIC_NUMS[i]);
//     }

//     std::println("Finding magic numbers for rooks");
//     for (int i = 0; i < 64; i++) {
//         ROOK_MAGIC_NUMS[i] = test_magic_numbers(i, false);
//         std::println("{}", ROOK_MAGIC_NUMS[i]);
//     }
// }

void init_sliding_piece_attack_tables() {
    // size_t bishop_size{0}; // always 5248
    // size_t rook_size{0}; // always 102400
    // for (int i = 0; i < 64; i++) {
    //     bishop_size += 1ULL << bishop_relevant_bits[i];
    //     rook_size += 1ULL << rook_relevant_bits[i];
    // }

    // Assigning memory offsets and populating attack bitboards
    U64* cur_b_offset = BISHOP_ATTACKS.data();
    U64* cur_r_offset = ROOK_ATTACKS.data();
    for (int i = 0; i < 64; i++) {
        BISHOP_ATTACK_PTRS[i] = cur_b_offset;
        ROOK_ATTACK_PTRS[i] = cur_r_offset;

        // Pre-calculating bishop attacks
        U64 b = 0ULL;
        do {
            int index = static_cast<int>((b * BISHOP_MAGIC_NUMS[i]) >> (64 - bishop_relevant_bits[i]));
            cur_b_offset[index] = calc_bishop_attack_ray(i, b);
            b = (b - BISHOP_ATTACK_MASK[i]) & BISHOP_ATTACK_MASK[i]; // Carry-rippling subset iteration
        } while (b);

        // Pre-calculating rook attacks
        b = 0ULL;
        do {
            int index = static_cast<int>((b * ROOK_MAGIC_NUMS[i]) >> (64 - rook_relevant_bits[i]));
            cur_r_offset[index] = calc_rook_attack_ray(i, b);
            b = (b - ROOK_ATTACK_MASK[i]) & ROOK_ATTACK_MASK[i]; // Carry-rippling subset iteration
        } while (b);

        cur_b_offset += 1ULL << bishop_relevant_bits[i];
        cur_r_offset += 1ULL << rook_relevant_bits[i];
    }
}
