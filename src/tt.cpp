#include "tt.h"

void TranspositionTable::resize(size_t size_mb) {
    num_entries = (size_mb * 1024 * 1024) / sizeof(TTEntry);
    table.clear();
    table.resize(num_entries);
}

void TranspositionTable::clear() {
    std::fill(table.begin(), table.end(), TTEntry{});
}

// Adjust mate scores for current ply distance from root
int TranspositionTable::score_to_tt(int score, int ply) {
    if (score > 30000)  return score + ply;
    if (score < -30000) return score - ply;
    return score;
}

int TranspositionTable::score_from_tt(int score, int ply) {
    if (score > 30000)  return score - ply;
    if (score < -30000) return score + ply;
    return score;
}

void TranspositionTable::store(uint64_t key, int depth, int score, TTFlag flag, Move move, int ply) {
    size_t index = key % num_entries;
    TTEntry& entry = table[index];

    // Replacement Strategy: Replace if shallow depth OR new position (overwrite)
    if (entry.key == 0 || depth >= entry.depth || key != entry.key || entry.age != current_age) {
        entry.key = key;
        entry.score = score_to_tt(score, ply);
        entry.depth = depth;
        entry.flag = flag;
        entry.move = move;
        entry.age = current_age;
    }
}

bool TranspositionTable::probe(uint64_t key, int depth, int alpha, int beta, int& score, Move& tt_move, int ply) {
    size_t index = key % num_entries;
    TTEntry& entry = table[index];

    if (entry.key == key) {
        tt_move = entry.move; // Always retrieve stored move for Move Ordering

        if (entry.depth >= depth) {
            int tt_score = score_from_tt(entry.score, ply);

            if (entry.flag == EXACT) {
                score = tt_score;
                return true;
            }
            if (entry.flag == LOWER && tt_score >= beta) {
                score = tt_score;
                return true;
            }
            if (entry.flag == UPPER && tt_score <= alpha) {
                score = tt_score;
                return true;
            }
        }
    }
    return false; // Useful move retrieved, but search depth insufficient to cut off
}