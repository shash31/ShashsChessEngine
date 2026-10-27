#pragma once

#include <random>
#include "types.h"
#include "utils.h"

// ============================================================================
// Helper functions and variables for finding magic numbers
// ============================================================================

// pseudo random number state
inline unsigned int state = 1804289383;

// Random number generators for magic number candidate
U64 get_random_U64_number();
U64 generate_magic_number();

U64 calc_bishop_attack_ray(int square, U64 blockers);
U64 calc_rook_attack_ray(int square, U64 blockers);

U64 test_magic_numbers(int square, bool isBishop);

// void init_magic_numbers();
void init_sliding_piece_attack_tables();
