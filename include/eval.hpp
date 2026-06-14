#pragma once
#include "state.hpp"
#include "movegen.hpp"
#include <cmath>
#include <iostream>

namespace Engine::Eval {

    // Standard and custom material values
    const int VAL_PAWN      = 100;
    const int VAL_PEASANT   = 70;
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
    const int VAL_PONTIFF   = 700; 
    const int VAL_SOLDIER   = 200; 
    const int VAL_JESTER    = 0;

    inline int get_piece_value(PieceID piece) {
        switch(piece) {
            case PEASANT:   return VAL_PEASANT;
            case PAWN:      return VAL_PAWN;
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
            case PONTIFF :  return VAL_PONTIFF;
            case JESTER:    return VAL_JESTER;
            default:        return 0;
        }
    }

    // Dynamic Piece-Square Positional Bonus
    inline int get_positional_bonus(int index, const BoardGeometry& geo, PieceID piece) {
        if (piece == JESTER || piece == POWDER_KEG) return 0;

        auto [x, y] = geo.index_to_coord(index);
        float center_x = geo.active_width / 2.0f;
        float center_y = geo.active_height / 2.0f;

        float dist_x = std::abs(x - center_x);
        float dist_y = std::abs(y - center_y);
        float max_dist = center_x + center_y; 
        
        float proximity = 1.0f - ((dist_x + dist_y) / max_dist);
        
        switch(piece) {
            case PEASANT:
            case KNIGHT:    return static_cast<int>(proximity * 60.0f);
            case BISHOP:    return static_cast<int>(proximity * 40.0f);
            case PONTIFF:   return static_cast<int>(proximity * 40.0f);
            case QUEEN:
            case ROOK:
            case TOWER:     return static_cast<int>(proximity * 10.0f);
            case KING:
            case REGENT:    return static_cast<int>(proximity * -150.0f); // Coward clause
            default:        return static_cast<int>(proximity * 20.0f);
        }
    }

    // Dynamic Threat Scanner
    inline int evaluate_threats(const BoardState& board, const BoardGeometry& geo) {
        int penalty = 0;

        // --- WHITE THREATS ---
        Bitboard192 w_culv = board.pieces[WHITE][CULVERIN];
        while(w_culv.popcount() > 0) {
            int idx = w_culv.lsb(); w_culv.clear_bit(idx);
            auto [x, y] = geo.index_to_coord(idx);
            if (y + 1 < geo.active_height && board.get_piece_at(geo.coord_to_index(x, y + 1), WHITE) != EMPTY) {
                penalty -= 150; 
            }
        }

        Bitboard192 w_king = board.pieces[WHITE][KING];
        if (w_king.popcount() > 0) {
            int idx = w_king.lsb();
            auto [x, y] = geo.index_to_coord(idx);
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                        if (board.get_piece_at(geo.coord_to_index(nx, ny), WHITE) == POWDER_KEG) penalty -= 500;
                    }
                }
            }
        }

        // --- BLACK THREATS ---
        Bitboard192 b_culv = board.pieces[BLACK][CULVERIN];
        while(b_culv.popcount() > 0) {
            int idx = b_culv.lsb(); b_culv.clear_bit(idx);
            auto [x, y] = geo.index_to_coord(idx);
            if (y - 1 >= 0 && board.get_piece_at(geo.coord_to_index(x, y - 1), BLACK) != EMPTY) {
                penalty += 150; 
            }
        }

        Bitboard192 b_king = board.pieces[BLACK][KING];
        if (b_king.popcount() > 0) {
            int idx = b_king.lsb();
            auto [x, y] = geo.index_to_coord(idx);
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                        if (board.get_piece_at(geo.coord_to_index(nx, ny), BLACK) == POWDER_KEG) penalty += 500;
                    }
                }
            }
        }

        return penalty;
    }

    // Combine Material, Positional, and Threat Evaluation
    inline int evaluate(const BoardState& board, const BoardGeometry& geo) {
        int score = 0;

        // Raw Material Counters
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

        score +=board.pieces[WHITE][PONTIFF].popcount() * VAL_PONTIFF;
        score -=board.pieces[BLACK][PONTIFF].popcount() * VAL_PONTIFF;

        // Positional Scanner
        Bitboard192 w_occ = board.occupancy[WHITE];
        while (w_occ.popcount() > 0) {
            int idx = w_occ.lsb(); w_occ.clear_bit(idx);
            score += get_positional_bonus(idx, geo, board.get_piece_at(idx, WHITE));
        }

        Bitboard192 b_occ = board.occupancy[BLACK];
        while (b_occ.popcount() > 0) {
            int idx = b_occ.lsb(); b_occ.clear_bit(idx);
            score -= get_positional_bonus(idx, geo, board.get_piece_at(idx, BLACK));
        }
        
        score += evaluate_threats(board, geo);
        
        return (board.turn == WHITE) ? score : -score;
    }

    // DIAGNOSTIC AI LOGGER
    inline void print_eval_breakdown(const BoardState& board, const BoardGeometry& geo) {
        int w_mat = 0, b_mat = 0;

        w_mat += board.pieces[WHITE][PEASANT].popcount() * VAL_PEASANT;
        b_mat += board.pieces[BLACK][PEASANT].popcount() * VAL_PEASANT;
        w_mat += board.pieces[WHITE][KNIGHT].popcount() * VAL_KNIGHT;
        b_mat += board.pieces[BLACK][KNIGHT].popcount() * VAL_KNIGHT;
        w_mat += board.pieces[WHITE][BISHOP].popcount() * VAL_BISHOP;
        b_mat += board.pieces[BLACK][BISHOP].popcount() * VAL_BISHOP;
        w_mat += board.pieces[WHITE][ROOK].popcount() * VAL_ROOK;
        b_mat += board.pieces[BLACK][ROOK].popcount() * VAL_ROOK;
        w_mat += board.pieces[WHITE][QUEEN].popcount() * VAL_QUEEN;
        b_mat += board.pieces[BLACK][QUEEN].popcount() * VAL_QUEEN;
        w_mat += board.pieces[WHITE][KING].popcount() * VAL_KING;
        b_mat += board.pieces[BLACK][KING].popcount() * VAL_KING;
        // <-- ADDED: Diagnostic Pontiff Logging
        w_mat += board.pieces[WHITE][PONTIFF].popcount() * VAL_PONTIFF;
        b_mat += board.pieces[BLACK][PONTIFF].popcount() * VAL_PONTIFF;
        w_mat += board.pieces[WHITE][PAWN].popcount() * VAL_PAWN;
        b_mat += board.pieces[BLACK][PAWN].popcount() * VAL_PAWN;

        int w_pos = 0, b_pos = 0;
        Bitboard192 w_occ = board.occupancy[WHITE];
        while (w_occ.popcount() > 0) {
            int idx = w_occ.lsb(); w_occ.clear_bit(idx);
            w_pos += get_positional_bonus(idx, geo, board.get_piece_at(idx, WHITE));
        }

        Bitboard192 b_occ = board.occupancy[BLACK];
        while (b_occ.popcount() > 0) {
            int idx = b_occ.lsb(); b_occ.clear_bit(idx);
            b_pos += get_positional_bonus(idx, geo, board.get_piece_at(idx, BLACK));
        }

        std::cout << "\n--- AI EVALUATION BREAKDOWN ---" << std::endl;
        std::cout << "White Material Score : " << w_mat << std::endl;
        std::cout << "Black Material Score : " << b_mat << std::endl;
        std::cout << "White Position Score : " << w_pos << std::endl;
        std::cout << "Black Position Score : " << b_pos << std::endl;
        std::cout << "Total Advantage      : " << evaluate(board, geo) << " (from active player POV)" << std::endl;
        std::cout << "-------------------------------\n" << std::endl;
    }
}