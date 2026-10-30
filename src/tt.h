#pragma once

#include <random>
#include "types.h"

enum TTFlag : uint8_t {
    EXACT, // Exact score (PV Node)
    LOWER, // Fail-High (Beta Cutoff) -> Score is at least this high
    UPPER  // Fail-Low (Alpha) -> Score is at most this high
};

struct TTEntry {
    uint64_t key;      // Zobrist key
    int16_t score;     // Eval or search score
    Move move;         // Best move
    int8_t depth;      // Search depth
    TTFlag flag;       // EXACT, LOWER, UPPER
    uint8_t age;       // When entry was made
};

class TranspositionTable {
    private:
        std::vector<TTEntry> table;
        size_t num_entries;

    public:
        void resize(size_t size_mb);

        void clear();

        // Adjust mate scores for current ply distance from root
        int score_to_tt(int score, int ply);

        int score_from_tt(int score, int ply);

        void store(uint64_t key, int depth, int score, TTFlag flag, Move move, int ply);

        bool probe(uint64_t key, int depth, int alpha, int beta, int& score, Move& tt_move, int ply);
};

inline uint8_t current_age{0};