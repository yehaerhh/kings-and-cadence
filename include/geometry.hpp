#pragma once
#include "bitboard.hpp"
#include <utility>
#include <vector>

namespace Engine {

    struct BoardGeometry {
        int active_width;
        int active_height;
        Bitboard192 in_bounds_mask;

        // Pre-computed Look-Up Tables (LUTs)
        Bitboard192 rank_masks[MAX_HEIGHT];
        Bitboard192 file_masks[MAX_WIDTH];
        Bitboard192 king_attacks[MAX_SQUARES];
        Bitboard192 knight_attacks[MAX_SQUARES];

        BoardGeometry(int width, int height) {
            active_width = width;
            active_height = height;
            in_bounds_mask = Bitboard192();

            // 1. Build In-Bounds Mask & Rank/File Masks
            for (int y = 0; y < active_height; ++y) {
                for (int x = 0; x < active_width; ++x) {
                    int idx = coord_to_index(x, y);
                    in_bounds_mask.set_bit(idx);
                    rank_masks[y].set_bit(idx);
                    file_masks[x].set_bit(idx);
                }
            }

            // 2. Build King & Knight Attack LUTs (Bounded by the walls)
            int king_dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
            int king_dy[] = {1, 1, 1, 0, 0, -1, -1, -1};
            
            int knight_dx[] = {-2, -1, 1, 2, -2, -1, 1, 2};
            int knight_dy[] = {1, 2, 2, 1, -1, -2, -2, -1};

            for (int y = 0; y < active_height; ++y) {
                for (int x = 0; x < active_width; ++x) {
                    int current_idx = coord_to_index(x, y);
                    
                    // King Moves
                    for (int i = 0; i < 8; ++i) {
                        int nx = x + king_dx[i];
                        int ny = y + king_dy[i];
                        if (nx >= 0 && nx < active_width && ny >= 0 && ny < active_height) {
                            king_attacks[current_idx].set_bit(coord_to_index(nx, ny));
                        }
                    }

                    // Knight Moves
                    for (int i = 0; i < 8; ++i) {
                        int nx = x + knight_dx[i];
                        int ny = y + knight_dy[i];
                        if (nx >= 0 && nx < active_width && ny >= 0 && ny < active_height) {
                            knight_attacks[current_idx].set_bit(coord_to_index(nx, ny));
                        }
                    }
                }
            }
        }

        inline int coord_to_index(int x, int y) const { return (y * MAX_WIDTH) + x; }
        inline std::pair<int, int> index_to_coord(int index) const { return {index % MAX_WIDTH, index / MAX_WIDTH}; }
        inline bool is_in_bounds(int index) const {
            if (index < 0 || index >= MAX_SQUARES) return false;
            return in_bounds_mask.test_bit(index);
        }
    };

} // namespace Engine