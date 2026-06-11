#pragma once
#include "state.hpp"
#include "geometry.hpp"
#include "move.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace Engine::MoveGen {

    // Helper: 114. The Jester Immunity Check
    inline bool is_jester(const BoardState& board, Color opp_color, int index) {
        return board.get_piece_at(index, opp_color) == JESTER;
    }

    // 108 & 109. Peasant (Pawn) Pushes and Captures
    inline void generate_peasant_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        
        // White is at bottom (Row 6) -> moves -12
        // Black is at top (Row 1) -> moves +12
        int forward = (us == WHITE) ? -12 : 12; 
        
        Bitboard192 peasants = board.pieces[us][PEASANT];
        
        while (peasants.popcount() > 0) {
            int from = peasants.lsb();
            peasants.clear_bit(from); 
            
            auto [x, y] = geo.index_to_coord(from);
            
            // 1. Single Push
            int push_to = from + forward;
            if (geo.is_in_bounds(push_to) && 
                !board.occupancy[WHITE].test_bit(push_to) && 
                !board.occupancy[BLACK].test_bit(push_to)) {
                
                moves.add(Move(from, push_to, PEASANT));
                
                // 2. THE DOUBLE PUSH
                if ((us == WHITE && y == 6) || (us == BLACK && y == 1)) {
                    int double_push = push_to + forward;
                    
                    if (geo.is_in_bounds(double_push) && 
                        !board.occupancy[WHITE].test_bit(double_push) && 
                        !board.occupancy[BLACK].test_bit(double_push)) {
                        
                        moves.add(Move(from, double_push, PEASANT));
                    }
                }
            }
            
            // 3. Diagonal Captures
            int caps[2] = { from + forward - 1, from + forward + 1 };
            for (int to : caps) {
                if (geo.is_in_bounds(to) && board.occupancy[them].test_bit(to)) {
                    auto [to_x, to_y] = geo.index_to_coord(to);
                    if (std::abs(to_x - x) == 1) { 
                        if (!is_jester(board, them, to)) { 
                            moves.add(Move(from, to, PEASANT, board.get_piece_at(to, them), FLAG_CAPTURE));
                        }
                    }
                }
            }
        }
    }

    // 113. LUT-based Stepping Moves (Knights, Kings, Regents)
    inline void generate_step_moves(const BoardState& board, MoveList& moves, Color us, PieceID piece, const Bitboard192* lut) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        Bitboard192 pieces = board.pieces[us][piece];
        
        while (pieces.popcount() > 0) {
            int from = pieces.lsb();
            pieces.clear_bit(from);
            
            Bitboard192 attacks = lut[from] & (~board.occupancy[us]);
            
            while (attacks.popcount() > 0) {
                int to = attacks.lsb();
                attacks.clear_bit(to);
                
                if (board.occupancy[them].test_bit(to)) {
                    if (!is_jester(board, them, to)) { 
                        moves.add(Move(from, to, piece, board.get_piece_at(to, them), FLAG_CAPTURE));
                    }
                } else {
                    moves.add(Move(from, to, piece));
                }
            }
        }
    }

    // Directional Vector Arrays for Ray-Casting
    inline const std::vector<std::pair<int, int>> ORTHOGONAL = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    inline const std::vector<std::pair<int, int>> DIAGONAL = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    inline const std::vector<std::pair<int, int>> OMNI = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    // Defines the sliding direction constraints
    enum SlideDirection {
        SLIDE_ORTHOGONAL,
        SLIDE_DIAGONAL,
        SLIDE_ALL         
    };

    // 110. Ray-Casting Sliding Move Generator
    inline void generate_sliding_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us, PieceID piece, SlideDirection dir_type, int max_range) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        Bitboard192 pieces = board.pieces[us][piece];
        
        const std::vector<std::pair<int, int>>* active_directions;
        if (dir_type == SLIDE_ORTHOGONAL) {
            active_directions = &ORTHOGONAL;
        } else if (dir_type == SLIDE_DIAGONAL) {
            active_directions = &DIAGONAL;
        } else {
            active_directions = &OMNI; 
        }
        
        while (pieces.popcount() > 0) {
            int from = pieces.lsb();
            pieces.clear_bit(from);
            
            auto [start_x, start_y] = geo.index_to_coord(from);
            
            for (auto [dx, dy] : *active_directions) {
                for (int step = 1; step <= max_range; ++step) {
                    int nx = start_x + (dx * step);
                    int ny = start_y + (dy * step);
                    
                    if (nx < 0 || nx >= geo.active_width || ny < 0 || ny >= geo.active_height) break;
                    
                    int to = geo.coord_to_index(nx, ny);
                    
                    if (board.occupancy[us].test_bit(to)) {
                        break; 
                    }
                    
                    if (board.occupancy[them].test_bit(to)) {
                        if (!is_jester(board, them, to)) { 
                            moves.add(Move(from, to, piece, board.get_piece_at(to, them), FLAG_CAPTURE));
                        }
                        break; 
                    }
                    
                    moves.add(Move(from, to, piece));
                }
            }
        }
    }

    // 122 & 123. Harpooner Vector Generator
    inline void generate_harpooner_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        Bitboard192 harpooners = board.pieces[us][HARPOONER];

        while (harpooners.popcount() > 0) {
            int from = harpooners.lsb();
            harpooners.clear_bit(from);
            auto [sx, sy] = geo.index_to_coord(from);

            for (auto [dx, dy] : OMNI) {
                int nx = sx + dx, ny = sy + dy;
                if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                    int to = geo.coord_to_index(nx, ny);
                    if (!board.occupancy[NONE].test_bit(to)) {
                        moves.add(Move(from, to, HARPOONER)); 
                    }
                }
            }

            for (auto [dx, dy] : OMNI) {
                int nx = sx + (dx * 3), ny = sy + (dy * 3);
                if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                    bool blocked = false;
                    for (int step = 1; step <= 2; ++step) {
                        if (board.occupancy[NONE].test_bit(geo.coord_to_index(sx + (dx * step), sy + (dy * step)))) {
                            blocked = true;
                        }
                    }
                    if (!blocked) {
                        int target_idx = geo.coord_to_index(nx, ny);
                        if (board.occupancy[them].test_bit(target_idx) && !is_jester(board, them, target_idx)) {
                            moves.add(Move(from, target_idx, HARPOONER, board.get_piece_at(target_idx, them), FLAG_DISPLACE));
                        }
                    }
                }
            }
        }
    }

    // 125 & 126. Culverin Line-Splash Generator
    inline void generate_culverin_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        Bitboard192 culverins = board.pieces[us][CULVERIN];

        while (culverins.popcount() > 0) {
            int from = culverins.lsb();
            culverins.clear_bit(from);
            auto [sx, sy] = geo.index_to_coord(from);

            for (auto [dx, dy] : OMNI) {
                for (int step = 1; step <= 3; ++step) {
                    int nx = sx + (dx * step), ny = sy + (dy * step);
                    if (nx < 0 || nx >= geo.active_width || ny < 0 || ny >= geo.active_height) break;
                    
                    int to = geo.coord_to_index(nx, ny);
                    if (board.occupancy[us].test_bit(to)) break; 
                    
                    if (board.occupancy[them].test_bit(to)) {
                        int splash_x = nx + dx, splash_y = ny + dy;
                        if (splash_x >= 0 && splash_x < geo.active_width && splash_y >= 0 && splash_y < geo.active_height) {
                            int splash_idx = geo.coord_to_index(splash_x, splash_y);
                            if (board.occupancy[them].test_bit(splash_idx) && !is_jester(board, them, splash_idx)) {
                                moves.add(Move(from, splash_idx, CULVERIN, board.get_piece_at(splash_idx, them), FLAG_SPLASH));
                            }
                        }
                        if (!is_jester(board, them, to)) {
                            moves.add(Move(from, to, CULVERIN, board.get_piece_at(to, them), FLAG_CAPTURE));
                        }
                        break; 
                    } else {
                        moves.add(Move(from, to, CULVERIN)); 
                    }
                }
            }
        }
    }

    // 127 & 128. Powder Keg Generator
    inline void generate_keg_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        Bitboard192 kegs = board.pieces[us][POWDER_KEG];

        while (kegs.popcount() > 0) {
            int from = kegs.lsb();
            kegs.clear_bit(from);
            auto [sx, sy] = geo.index_to_coord(from);

            for (auto [dx, dy] : OMNI) {
                int nx = sx + dx, ny = sy + dy;
                if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                    int to = geo.coord_to_index(nx, ny);
                    
                    if (!board.occupancy[NONE].test_bit(to)) {
                        moves.add(Move(from, to, POWDER_KEG)); 
                    } else {
                        Color target_color = board.occupancy[us].test_bit(to) ? us : them;
                        if (!is_jester(board, target_color, to)) { 
                            PieceID captured = board.get_piece_at(to, target_color);
                            moves.add(Move(from, to, POWDER_KEG, captured, FLAG_SPLASH));
                        }
                    }
                }
            }
        }
    }

    // 135. Jamming Checker
    inline bool is_bandit_jammed(const BoardState& board, const BoardGeometry& geo, int index, Color them) {
        Bitboard192 adjacent = geo.king_attacks[index];
        return (adjacent & board.occupancy[them]).popcount() > 0;
    }

    // 137. Internal Recursive Loop for Bandit Chain Dashes
    inline void bandit_dfs(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us, int start_idx, int current_idx, Bitboard192 enemy_occ, std::vector<int>& current_chain, int depth) {
        if (depth > 12) return; 

        Color them = (us == WHITE) ? BLACK : WHITE;
        auto [sx, sy] = geo.index_to_coord(current_idx);
        bool made_jump = false;

        for (int dx = -3; dx <= 3; ++dx) {
            for (int dy = -3; dy <= 3; ++dy) {
                int dist = std::max(std::abs(dx), std::abs(dy)); 
                if (dist == 2 || dist == 3) {
                    int nx = sx + dx, ny = sy + dy;
                    if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                        int to = geo.coord_to_index(nx, ny);
                        
                        if (!board.occupancy[us].test_bit(to) && enemy_occ.test_bit(to) && !is_jester(board, them, to)) {
                            PieceID target = board.get_piece_at(to, them);
                            
                            std::vector<int> new_chain = current_chain;
                            new_chain.push_back(to);
                            
                            if (target == PEASANT) {
                                Bitboard192 next_occ = enemy_occ;
                                next_occ.clear_bit(to);
                                bandit_dfs(board, geo, moves, us, start_idx, to, next_occ, new_chain, depth + 1);
                                made_jump = true;
                            } else {
                                moves.add_macro(Move(start_idx, to, BANDIT, target, FLAG_CHAIN_DASH), new_chain);
                                made_jump = true;
                            }
                        }
                    }
                }
            }
        }
        
        if (!made_jump && !current_chain.empty()) {
            int final_target = current_chain.back();
            PieceID final_target_piece = board.get_piece_at(final_target, them);
            moves.add_macro(Move(start_idx, current_idx, BANDIT, final_target_piece, FLAG_CHAIN_DASH), current_chain);
        }
    }

    inline void generate_bandit_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        Bitboard192 bandits = board.pieces[us][BANDIT];

        while (bandits.popcount() > 0) {
            int from = bandits.lsb();
            bandits.clear_bit(from);

            if (is_bandit_jammed(board, geo, from, them)) continue;

            Bitboard192 steps = geo.king_attacks[from] & (~board.occupancy[NONE]);
            while (steps.popcount() > 0) {
                int to = steps.lsb();
                steps.clear_bit(to);
                moves.add(Move(from, to, BANDIT));
            }

            std::vector<int> initial_chain;
            bandit_dfs(board, geo, moves, us, from, from, board.occupancy[them], initial_chain, 0);
        }
    }

    // 116. Reverse-Vector Attack Scanner
    inline bool is_square_attacked(const BoardState& board, const BoardGeometry& geo, int target_index, Color attacker_color) {
        Color defender_color = (attacker_color == WHITE) ? BLACK : WHITE;
        
        if ((geo.knight_attacks[target_index] & board.pieces[attacker_color][KNIGHT]).popcount() > 0) return true;
        if ((geo.king_attacks[target_index] & (board.pieces[attacker_color][KING] | board.pieces[attacker_color][REGENT])).popcount() > 0) return true;
        
        // Use proper 12-stride offsets so King safety actually respects Peasant captures
        int reverse_dir = (attacker_color == WHITE) ? 12 : -12;
        int p_caps[2] = { target_index + reverse_dir - 1, target_index + reverse_dir + 1 };
        for (int from : p_caps) {
            if (geo.is_in_bounds(from) && board.pieces[attacker_color][PEASANT].test_bit(from)) return true;
        }

        auto [tx, ty] = geo.index_to_coord(target_index);
        for (auto [dx, dy] : ORTHOGONAL) {
            for (int step = 1; step <= 99; ++step) {
                int nx = tx + (dx * step);
                int ny = ty + (dy * step);
                if (nx < 0 || nx >= geo.active_width || ny < 0 || ny >= geo.active_height) break;
                
                int scan_idx = geo.coord_to_index(nx, ny);
                if (board.occupancy[defender_color].test_bit(scan_idx)) break; 
                if (board.occupancy[attacker_color].test_bit(scan_idx)) {
                    if (board.get_piece_at(scan_idx, attacker_color) == TOWER) return true;
                    break; 
                }
            }
        }
        return false;
    }

    // 117. Master Pseudo-Legal Generator
    inline void generate_pseudo_legal_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        generate_peasant_moves(board, geo, moves, us);
        generate_step_moves(board, moves, us, KNIGHT, geo.knight_attacks);
        generate_step_moves(board, moves, us, KING, geo.king_attacks);
        
        // --- STANDARD SLIDERS ---
        generate_sliding_moves(board, geo, moves, us, ROOK, SLIDE_ORTHOGONAL, 99);
        generate_sliding_moves(board, geo, moves, us, BISHOP, SLIDE_DIAGONAL, 99);
        generate_sliding_moves(board, geo, moves, us, QUEEN, SLIDE_ALL, 99);

        // --- Custom Pieces ---
        generate_step_moves(board, moves, us, REGENT, geo.king_attacks);
        generate_sliding_moves(board, geo, moves, us, TOWER, SLIDE_ORTHOGONAL, 99);
        generate_harpooner_moves(board, geo, moves, us);
        generate_culverin_moves(board, geo, moves, us);
        generate_keg_moves(board, geo, moves, us); 
        generate_bandit_moves(board, geo, moves, us); 
    }

    // 115. Filter Safe King (Absolute Pin logic)
    inline void generate_legal_moves(const BoardState& board, const BoardGeometry& geo, MoveList& legal_moves, Color us) {
        MoveList pseudo;
        generate_pseudo_legal_moves(board, geo, pseudo, us);

        Color them = (us == WHITE) ? BLACK : WHITE;

        for (size_t i = 0; i < pseudo.size(); i++) {
            Move m = pseudo.moves[i];
            
            BoardState test_board = board;
            
            if (m.get_flag() == FLAG_CAPTURE) {
                test_board.remove_piece(them, m.get_captured(), m.get_to());
            }
            test_board.move_piece(us, m.get_piece(), m.get_from(), m.get_to());

            int king_idx = test_board.pieces[us][KING].lsb();
            
            if (king_idx == -1 || !is_square_attacked(test_board, geo, king_idx, them)) {
                legal_moves.add(m);
            }
        }
    }

    // 179. Tactical Move Generator for Quiescence Search
    inline void generate_tactical_moves(BoardState& board, const BoardGeometry& geo, MoveList& tactical_moves, Color us) {
        MoveList all_legal_moves;
        generate_legal_moves(board, geo, all_legal_moves, us);

        for (size_t i = 0; i < all_legal_moves.size(); ++i) {
            Move m = all_legal_moves.moves[i];
            
            bool is_tactical = (m.get_captured() != EMPTY) || 
                               (m.get_flag() == FLAG_SPLASH) || 
                               (m.get_flag() == FLAG_DISPLACE) || 
                               (m.get_flag() == FLAG_CHAIN_DASH);

            if (is_tactical) {
                tactical_moves.moves.push_back(m);
                if (i < all_legal_moves.macro_chains.size()) {
                    tactical_moves.macro_chains.push_back(all_legal_moves.macro_chains[i]);
                } else {
                    tactical_moves.macro_chains.push_back({});
                }
            }
        }
    }
}