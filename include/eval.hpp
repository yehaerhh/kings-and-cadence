#pragma once
#include "state.hpp"
#include <cmath> // Needed for std::abs

namespace Engine::Eval {

    // 190 & 191: Define standard and custom material values
    const int VAL_PEASANT   = 100;
    const int VAL_KNIGHT    = 300;
    const int VAL_BISHOP    = 320;   
    const int VAL_ROOK      = 500;   
    const int VAL_TOWER     = 500;
    const int VAL_QUEEN     = 900;   
    const int VAL_KING      = 20000; 
    const int VAL_REGENT    = 900;   
    const int VAL_BANDIT    = 450;
    const int VAL_HARPOONER = 350;
    const int VAL_CULVERIN  = 400;
    const int VAL_POWDER_KEG= 200;   
    const int VAL_JESTER    = 0;

    // Helper to map PieceID to Value
    inline int get_piece_value(PieceID piece) {
        switch(piece) {
            case PEASANT:   return VAL_PEASANT;
            case KNIGHT:    return VAL_KNIGHT;
            case BISHOP:    return VAL_BISHOP; 
            case ROOK:      return VAL_ROOK;   
            case TOWER:     return VAL_TOWER;
            case QUEEN:     return VAL_QUEEN;  
            case KING:      return VAL_KING;
            case REGENT:    return VAL_REGENT;
            case BANDIT:    return VAL_BANDIT;
            case HARPOONER: return VAL_HARPOONER;
            case CULVERIN:  return VAL_CULVERIN;
            case POWDER_KEG:return VAL_POWDER_KEG;
            case JESTER:    return VAL_JESTER;
            default:        return 0;
        }
    }

    // --- ARCHITECTURAL FIX 2: Piece-Specific Positional AI ---
    // This tells the search algorithm WHERE pieces belong on the board.
    inline int get_positional_bonus(int index, const BoardGeometry& geo, PieceID piece) {
        if (piece == JESTER || piece == POWDER_KEG) return 0;

        auto [x, y] = geo.index_to_coord(index);
        float center_x = geo.active_width / 2.0f;
        float center_y = geo.active_height / 2.0f;

        float dist_x = std::abs(x - center_x);
        float dist_y = std::abs(y - center_y);
        float max_dist = center_x + center_y; 
        
        // Proximity is 1.0 at dead center, 0.0 at the extreme corners
        float proximity = 1.0f - ((dist_x + dist_y) / max_dist);
        
        switch(piece) {
            case PEASANT:
            case KNIGHT:
                // Minor pieces and pawns MUST fight for the center early! (+60 max)
                return static_cast<int>(proximity * 60.0f);
            case BISHOP:
                // Bishops like long diagonals cutting through the center (+40 max)
                return static_cast<int>(proximity * 40.0f);
            case QUEEN:
            case ROOK:
            case TOWER:
                // Major pieces should develop, but aren't desperate for dead center (+10 max)
                return static_cast<int>(proximity * 10.0f);
            case KING:
            case REGENT:
                // THE COWARD CLAUSE: The King must hide! 
                // Massive penalty (-150) for walking into the center of the board.
                return static_cast<int>(proximity * -150.0f);
            default:
                return static_cast<int>(proximity * 20.0f);
        }
    }

    // 195. Dynamic Threat and Vulnerability Scanning
    inline int evaluate_threats(const BoardState& board, const BoardGeometry& geo) {
        int penalty = 0;

        for (int i = 0; i < geo.active_width * geo.active_height; ++i) {
            PieceID w_piece = board.get_piece_at(i, WHITE);
            PieceID b_piece = board.get_piece_at(i, BLACK);
            auto [x, y] = geo.index_to_coord(i);

            // --- WHITE THREATS ---
            if (w_piece != EMPTY) {
                if (w_piece == CULVERIN && y + 1 < geo.active_height) {
                    if (board.get_piece_at(geo.coord_to_index(x, y + 1), WHITE) != EMPTY) {
                        penalty -= 150; 
                    }
                } else if (w_piece == KING) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                                if (board.get_piece_at(geo.coord_to_index(nx, ny), WHITE) == POWDER_KEG) {
                                    penalty -= 500; 
                                }
                            }
                        }
                    }
                }
            }

            // --- BLACK THREATS ---
            if (b_piece != EMPTY) {
                if (b_piece == CULVERIN && y - 1 >= 0) {
                    if (board.get_piece_at(geo.coord_to_index(x, y - 1), BLACK) != EMPTY) {
                        penalty += 150; 
                    }
                } else if (b_piece == KING) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                                if (board.get_piece_at(geo.coord_to_index(nx, ny), BLACK) == POWDER_KEG) {
                                    penalty += 500; 
                                }
                            }
                        }
                    }
                }
            }
        }
        return penalty;
    }

    // 193 & 194: Combine Material and Positional Evaluation
    inline int evaluate(const BoardState& board, const BoardGeometry& geo) {
        int score = 0;

        // --- ARCHITECTURAL FIX 1: Explicit Bitboard Counting ---
        // By removing the for loop, we guarantee C++ never tries to read an 
        // undefined PieceID index, entirely preventing the memory overflow 
        // that was causing all evaluations to silently crash to 0.
        
        score += board.pieces[WHITE][PEASANT].popcount() * VAL_PEASANT;
        score -= board.pieces[BLACK][PEASANT].popcount() * VAL_PEASANT;

        score += board.pieces[WHITE][KNIGHT].popcount() * VAL_KNIGHT;
        score -= board.pieces[BLACK][KNIGHT].popcount() * VAL_KNIGHT;

        score += board.pieces[WHITE][BISHOP].popcount() * VAL_BISHOP;
        score -= board.pieces[BLACK][BISHOP].popcount() * VAL_BISHOP;

        score += board.pieces[WHITE][ROOK].popcount() * VAL_ROOK;
        score -= board.pieces[BLACK][ROOK].popcount() * VAL_ROOK;

        score += board.pieces[WHITE][QUEEN].popcount() * VAL_QUEEN;
        score -= board.pieces[BLACK][QUEEN].popcount() * VAL_QUEEN;

        score += board.pieces[WHITE][KING].popcount() * VAL_KING;
        score -= board.pieces[BLACK][KING].popcount() * VAL_KING;

        // Custom Pieces
        score += board.pieces[WHITE][TOWER].popcount() * VAL_TOWER;
        score -= board.pieces[BLACK][TOWER].popcount() * VAL_TOWER;
        score += board.pieces[WHITE][REGENT].popcount() * VAL_REGENT;
        score -= board.pieces[BLACK][REGENT].popcount() * VAL_REGENT;
        score += board.pieces[WHITE][BANDIT].popcount() * VAL_BANDIT;
        score -= board.pieces[BLACK][BANDIT].popcount() * VAL_BANDIT;
        score += board.pieces[WHITE][HARPOONER].popcount() * VAL_HARPOONER;
        score -= board.pieces[BLACK][HARPOONER].popcount() * VAL_HARPOONER;
        score += board.pieces[WHITE][CULVERIN].popcount() * VAL_CULVERIN;
        score -= board.pieces[BLACK][CULVERIN].popcount() * VAL_CULVERIN;
        score += board.pieces[WHITE][POWDER_KEG].popcount() * VAL_POWDER_KEG;
        score -= board.pieces[BLACK][POWDER_KEG].popcount() * VAL_POWDER_KEG;

        // Dynamic Positional Value
        for (int i = 0; i < geo.active_width * geo.active_height; ++i) {
            PieceID w_piece = board.get_piece_at(i, WHITE);
            if (w_piece != EMPTY) {
                score += get_positional_bonus(i, geo, w_piece);
            }

            PieceID b_piece = board.get_piece_at(i, BLACK);
            if (b_piece != EMPTY) {
                score -= get_positional_bonus(i, geo, b_piece);
            }
        }
        
        score += evaluate_threats(board, geo);
        
        return (board.turn == WHITE) ? score : -score;
    }
}