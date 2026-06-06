#pragma once

namespace Engine {
    enum Color : int { WHITE = 0, BLACK = 1, NONE = 2 };
    
    // Standard pieces (0-5) + Custom Archetypes (6-13)
    enum PieceID : int {
        KING = 0, QUEEN = 1, ROOK = 2, BISHOP = 3, KNIGHT = 4, PAWN = 5,
        PEASANT = 6, BANDIT = 7, CULVERIN = 8, HARPOONER = 9,
        POWDER_KEG = 10, TOWER = 11, REGENT = 12, JESTER = 13,
        EMPTY = 14
    };

    // Constants for array sizing
    constexpr int MAX_COLORS = 2;
    constexpr int MAX_PIECE_TYPES = 16; // Padded for future custom pieces
}