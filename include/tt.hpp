#pragma once
#include "types.hpp"
#include <vector>

namespace Engine {
    enum EntryType { EXACT, LOWERBOUND, UPPERBOUND };

    struct TTEntry {
        uint64_t key;     // 64-bit hash from your Zobrist Hashing
        int16_t score;
        int8_t depth;
        EntryType type;
        Move best_move;
    };

    class TranspositionTable {
        std::vector<TTEntry> table;
    public:
        TranspositionTable(size_t size) : table(size) {}
        
        inline void store(uint64_t key, int depth, int16_t score, EntryType type, Move best_move) {
            size_t idx = key % table.size();
            table[idx] = {key, score, (int8_t)depth, type, best_move};
        }

        inline bool probe(uint64_t key, int depth, int16_t& score, EntryType& type, Move& best_move) {
            TTEntry& entry = table[key % table.size()];
            if (entry.key == key && entry.depth >= depth) {
                score = entry.score;
                type = entry.type;
                best_move = entry.best_move;
                return true;
            }
            return false;
        }
    };
}