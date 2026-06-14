#pragma once
#include <cstdint>
#include <vector>
#include "types.hpp"

namespace Engine {

    // Move Flags for your custom mechanics
    enum MoveFlag : int {
        FLAG_NONE = 0,
        FLAG_CAPTURE = 1,
        FLAG_SPLASH = 2,       // Culverin / Powder Keg
        FLAG_DISPLACE = 3,     // Harpooner
        FLAG_CHAIN_DASH = 4,   // Bandit
        FLAG_EN_PASSANT = 5,
        FLAG_CASTLE = 6,
        FLAG_PROMOTION = 7
    };


    // 106. Highly packed 32-bit Move struct
    // Bits 0-7:   From Square (0-143)
    // Bits 8-15:  To Square (0-143)
    // Bits 16-19: Moving Piece (0-15)
    // Bits 20-23: Captured Piece (0-15)
    // Bits 24-27: Move Flag (0-15)
    struct Move {
        uint32_t data;

        Move(){}
        // --- THE CONSTRUCTOR (Packing the bits) ---
        // You MUST update the bit-shifts here so they don't overwrite each other!
        Move(int f, int t, PieceID p, PieceID c = EMPTY, MoveFlag fl = FLAG_NONE) {
            data = f | (t << 8) | (p << 16) | (c << 21) | (fl << 26);
        }

        // --- THE GETTERS (Unpacking the bits) ---
        inline int get_from() const { return data & 0xFF; }
        inline int get_to() const { return (data >> 8) & 0xFF; }
        
        // 0x1F allows up to 31.
        inline PieceID get_piece() const { return static_cast<PieceID>((data >> 16) & 0x1F); }
        
        // Shifted from 20 to 21 to make room for the 5th piece bit!
        inline PieceID get_captured() const { return static_cast<PieceID>((data >> 21) & 0x1F); }
        
        // Shifted from 24 to 26!
        inline MoveFlag get_flag() const { return static_cast<MoveFlag>((data >> 26) & 0xF); }
    };

    // 107 & 140. MoveList upgraded to support MacroMoves
    struct MoveList {
        std::vector<Move> moves;
        // Stores the exact path of captured indices for Chain Dashes
        std::vector<std::vector<int>> macro_chains; 
        
        MoveList() {
            moves.reserve(256); 
            macro_chains.reserve(64); // Pre-allocate to prevent heap fragmentation
        }
        
        inline void add(const Move& m) { 
            moves.push_back(m); 
            macro_chains.push_back({}); // Keep arrays perfectly synced!
        }
        // 140 & 141. Package the sequence into a macro object
        inline void add_macro(const Move& m, const std::vector<int>& chain) {
            moves.push_back(m);
            macro_chains.push_back(chain);
        }
        
        inline void clear() { 
            moves.clear(); 
            macro_chains.clear(); 
        }
        inline size_t size() const { return moves.size(); }
    };

} // namespace Engine