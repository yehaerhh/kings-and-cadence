#pragma once
#include "types.hpp"
#include <vector>

namespace Engine {
    enum EntryType { EXACT, LOWERBOUND, UPPERBOUND };

    const int MATE_VALUE = 29000;
    const int MATE_BOUND = 28000;

    struct TTEntry {
        uint64_t key;     // 64-bit hash from your Zobrist Hashing
        int16_t score;
        int8_t depth;
        EntryType type;
        Move best_move;
    };

    class TranspositionTable {
    private:
        std::vector<TTEntry> table;

        inline int16_t score_to_tt(int16_t score, int ply){
            if(score> MATE_BOUND) return score +ply;
            if(score<-MATE_BOUND) return score -ply;
            return score;
        }

        inline int16_t score_from_tt(int16_t score , int ply){
            if(score> MATE_BOUND) return score -ply;
            if(score<-MATE_BOUND) return score +ply;
            return score;
        }
    public:
        TranspositionTable(size_t size) : table(size) {}
        
        inline bool probe(uint64_t key, int depth, int16_t& score, EntryType& type, Move& best_move) {
            TTEntry& entry = table[key % table.size()];
            
            // If the hash key matches, we have seen this position before
            if (entry.key == key) {
                // --- TASK 3 FIX --- 
                // ALWAYS extract the best move for sorting, regardless of the depth!
                best_move = entry.best_move; 
                
                // Only return true (triggering a full prune) if the depth is sufficient
                if (entry.depth >= depth) {
                    score = score_from_tt(entry.score,depth);
                    type = entry.type;
                    return true;
                }
            }
            return false;
        }

        inline void store(uint64_t key, int depth, int16_t score, EntryType type, Move best_move) {
            TTEntry& entry = table[key % table.size()];
            
            // --- THE FIX: BAN MATE BOUNDS ---
            // If the score is a checkmate score, but the type is NOT 'EXACT', 
            // do not store it! This prevents LMR and NMP from poisoning the TT 
            // with fake "Upperbound Mate" scores that cascade and ruin other branches.
            if ((score > MATE_BOUND || score < -MATE_BOUND) && type != EXACT) {
                return; // Refuse to poison the table
            }

            // Depth-Preferred Replacement Strategy
            if (entry.key != key || depth >= entry.depth) {
                entry.key = key;
                entry.depth = depth;
                // Adjust the mate score relative to depth before storing it!
                entry.score = score_to_tt(score, depth);
                entry.type = type;
                entry.best_move = best_move;
            }
        }
        
    };
}