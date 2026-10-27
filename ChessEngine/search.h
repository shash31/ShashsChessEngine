#pragma once

#include <chrono>
#include "tt.h"
#include "Board.h"
#include "eval.h"

constexpr int MATE = 32000;

inline uint64_t total_nodes = 0;
inline uint64_t nodes = 0;

inline TranspositionTable tt;

inline std::chrono::time_point<std::chrono::steady_clock> start_time;
inline double soft_limit_ms = 0;
inline double hard_limit_ms = 0;

inline int depth_limit = -1;

inline std::atomic<bool> stopSearch{false};

inline std::array<std::array<int, 64>, 64> lmr_table{};
inline void calc_lmr_table() {
    for (int d = 1; d < 64; d++)
        for (int m = 1; m < 64; m++)
            lmr_table[d][m] = int(0.75 + std::log(d) * std::log(m) / 2.25);
}


inline void check_time();
int quiescence(int ply, int alpha, int beta, Board& board);
int negamax(int depth, int ply, Board& board, int alpha, int beta);
Move ids(Board& board);
