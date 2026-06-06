#pragma once
#include "bitboard.hpp"
#include "types.hpp"
#include "zobrist.hpp"
#include <vector>

namespace Engine {
    
    // Tracks history so we can unmake moves perfectly during AI Search
    struct StateHistory {
        uint64_t hash_key;
        int half_move_clock;
        uint64_t castling_rights;
        int ep_target_index;
        PieceID captured_piece;
    };

    struct BoardState {
        // [2 Colors][16 Piece Types] = 32 separate 192-bit matrices
        Bitboard192 pieces[MAX_COLORS][MAX_PIECE_TYPES]; 
        
        // occupancy[WHITE]=0, occupancy[BLACK]=1, occupancy[NONE]=2 (Used as BOTH)
        Bitboard192 occupancy[3]; 

        Color turn = WHITE;
        uint64_t hash_key = 0ULL;
        int half_move_clock = 0;
        int full_move_number = 1;
        int ep_target_index = -1;
        uint64_t castling_rights = 0ULL;

        std::vector<StateHistory> history;

        BoardState() {
            history.reserve(256); // Prevent slow memory allocations during deep AI search
            clear();
        }

        void clear() {
            for(int c = 0; c < MAX_COLORS; c++) {
                for(int p = 0; p < MAX_PIECE_TYPES; p++) {
                    pieces[c][p] = Bitboard192();
                }
                occupancy[c] = Bitboard192();
            }
            occupancy[NONE] = Bitboard192(); // Using NONE (index 2) to store total occupancy
            hash_key = 0ULL;
            turn = WHITE;
            half_move_clock = 0;
            full_move_number = 1;
            ep_target_index = -1;
            castling_rights = 0ULL;
            history.clear();
        }

        // Extremely fast piece lookup for a specific square and color
        inline PieceID get_piece_at(int index, Color c) const {
            // If there's no piece of this color here, skip the loop instantly
            if (!occupancy[c].test_bit(index)) return EMPTY; 

            for(int p = 0; p < MAX_PIECE_TYPES; p++) {
                if (pieces[c][p].test_bit(index)) return static_cast<PieceID>(p);
            }
            return EMPTY;
        }

        // Unified Add/Remove functions that keep everything perfectly synced
        inline void add_piece(Color c, PieceID p, int index) {
            pieces[c][p].set_bit(index);
            occupancy[c].set_bit(index);
            occupancy[NONE].set_bit(index); 
            HashEngine::toggle_piece(hash_key, c, p, index);
        }

        inline void remove_piece(Color c, PieceID p, int index) {
            pieces[c][p].clear_bit(index);
            occupancy[c].clear_bit(index);
            occupancy[NONE].clear_bit(index);
            HashEngine::toggle_piece(hash_key, c, p, index);
        }

        // Task 58: Move a piece efficiently
        inline void move_piece(Color c, PieceID p, int from_index, int to_index) {
            // Because our hash toggle uses XOR, we can safely just call remove then add. 
            // The compiler will inline this into blazing fast assembly.
            remove_piece(c, p, from_index);
            add_piece(c, p, to_index);
        }
    };

} // namespace Engine