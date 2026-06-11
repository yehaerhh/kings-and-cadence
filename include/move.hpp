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
        // Constructor automatically bit-shifts the data into place
        Move(int from, int to, PieceID piece, PieceID captured = EMPTY, MoveFlag flag = FLAG_NONE) {
            data = (from & 0xFF) | 
                   ((to & 0xFF) << 8) | 
                   ((piece & 0xF) << 16) | 
                   ((captured & 0xF) << 20) | 
                   ((flag & 0xF) << 24);
        }

        // Inline getters reverse the bit-shifts instantly
        inline int get_from() const { return data & 0xFF; }
        inline int get_to() const { return (data >> 8) & 0xFF; }
        inline PieceID get_piece() const { return static_cast<PieceID>((data >> 16) & 0xF); }
        inline PieceID get_captured() const { return static_cast<PieceID>((data >> 20) & 0xF); }
        inline MoveFlag get_flag() const { return static_cast<MoveFlag>((data >> 24) & 0xF); }
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