#pragma once
#include <cstdint>
#include <random>
#include "types.hpp"
#include "geometry.hpp"

namespace Engine::HashEngine {

    // The Zobrist Lookup Tables
    inline uint64_t piece_keys[MAX_COLORS][MAX_PIECE_TYPES][MAX_SQUARES];
    inline uint64_t side_key;
    inline uint64_t castling_keys[16];
    inline uint64_t ep_keys[MAX_WIDTH]; // Target squares for En Passant / Dashes

    // Initialize the tables with a deterministic PRNG
    inline void init_zobrist() {
        // We use a fixed seed (0x123456789ABCDEF) so debugging is mathematically repeatable
        std::mt19937_64 rng(0x123456789ABCDEFULL); 

        for(int c = 0; c < MAX_COLORS; c++) {
            for(int p = 0; p < MAX_PIECE_TYPES; p++) {
                for(int s = 0; s < MAX_SQUARES; s++) {
                    piece_keys[c][p][s] = rng();
                }
            }
        }
        side_key = rng();
        for(int i = 0; i < 16; i++) castling_keys[i] = rng();
        for(int i = 0; i < MAX_WIDTH; i++) ep_keys[i] = rng();
    }

    // XOR toggles (Adding a piece and removing a piece use the exact same XOR operation)
    inline void toggle_piece(uint64_t& hash, Color c, PieceID p, int index) {
        hash ^= piece_keys[c][p][index];
    }
    
    inline void toggle_side(uint64_t& hash) {
        hash ^= side_key;
    }

    inline void toggle_ep(uint64_t& hash, int file) {
        hash ^= ep_keys[file];
    }

    inline void toggle_castling(uint64_t& hash, int castling_rights) {
        hash ^= castling_keys[castling_rights];
    }
}