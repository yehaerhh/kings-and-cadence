#pragma once
#include "state.hpp"
#include "geometry.hpp"
#include "move.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace Engine::MoveGen {

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

    // Helper: 114. The Jester Immunity Check
    inline bool is_jester(const BoardState& board, Color opp_color, int index) {
        return board.get_piece_at(index, opp_color) == JESTER;
    }
    
    // 116. Reverse-Vector Attack Scanner (NOW WITH QUEEN, BISHOP & PONTIFF SUPPORT)
    inline bool is_square_attacked(const BoardState& board, const BoardGeometry& geo, int target_index, Color attacker_color) {
        Color defender_color = (attacker_color == WHITE) ? BLACK : WHITE;
        
        // 1. KNIGHTS, KINGS, AND SOLDIERS (All use fixed-step geometry)
        if ((geo.knight_attacks[target_index] & board.pieces[attacker_color][KNIGHT]).popcount() > 0) return true;
        
        // ADDED SOLDIER HERE: It attacks exactly like a King
        Bitboard192 royals_and_soldiers = board.pieces[attacker_color][KING] | 
                                          board.pieces[attacker_color][REGENT] | 
                                          board.pieces[attacker_color][SOLDIER];
        if ((geo.king_attacks[target_index] & royals_and_soldiers).popcount() > 0) return true;
        
        // 2. PEASANTS
        // FIX: Use geo.active_width instead of 12!
        int forward = (attacker_color == WHITE) ? geo.active_width : -geo.active_width;
        int p_caps[2] = { target_index + forward - 1, target_index + forward + 1 };
        for (int from : p_caps) {
            if (geo.is_in_bounds(from) && board.pieces[attacker_color][PEASANT].test_bit(from)) return true;
        }

        auto [tx, ty] = geo.index_to_coord(target_index);
        // Orthogonal Checks (Rook / Queen / Tower)
        std::vector<std::pair<int, int>> ortho = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
        for (auto [dx, dy] : ortho) {
            for (int step = 1; step <= 99; ++step) {
                int nx = tx + (dx * step);
                int ny = ty + (dy * step);
                if (nx < 0 || nx >= geo.active_width || ny < 0 || ny >= geo.active_height) break;
                
                int scan_idx = geo.coord_to_index(nx, ny);
                if (board.occupancy[defender_color].test_bit(scan_idx)) break; 
                if (board.occupancy[attacker_color].test_bit(scan_idx)) {
                    PieceID p = board.get_piece_at(scan_idx, attacker_color);
                    if (p == TOWER || p == ROOK || p == QUEEN) return true;
                    break; 
                }
            }
        }
        
        // Diagonal Checks (Bishop / Queen)
        std::vector<std::pair<int, int>> diag = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (auto [dx, dy] : diag) {
            for (int step = 1; step <= 99; ++step) {
                int nx = tx + (dx * step);
                int ny = ty + (dy * step);
                if (nx < 0 || nx >= geo.active_width || ny < 0 || ny >= geo.active_height) break;
                
                int scan_idx = geo.coord_to_index(nx, ny);
                if (board.occupancy[defender_color].test_bit(scan_idx)) break; 
                if (board.occupancy[attacker_color].test_bit(scan_idx)) {
                    PieceID p = board.get_piece_at(scan_idx, attacker_color);
                    if (p == BISHOP || p == QUEEN) return true;
                    break; 
                }
            }
        }

        // --- NEW: PONTIFF ATTACK SENSOR ---
        int pontiff_dirs[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        for (int d = 0; d < 4; ++d) {
            int cx = tx;
            int cy = ty;
            int dx = pontiff_dirs[d][0];
            int dy = pontiff_dirs[d][1];
            
            for (int step = 0; step < 25; ++step) {
                int nx = cx + dx;
                int ny = cy + dy;
                
                // Wall bounce logic
                if (nx < 0 || nx >= geo.active_width) { dx = -dx; nx = cx + dx; }
                if (ny < 0 || ny >= geo.active_height) { dy = -dy; ny = cy + dy; }
                
                int scan_idx = geo.coord_to_index(nx, ny);
                
                // If it bounced perfectly back to the square we are scanning FROM, kill this ray
                if (scan_idx == target_index) break; 
                
                // Did the ray hit ANY piece on the board?
                if (board.get_piece_at(scan_idx, WHITE) != EMPTY || board.get_piece_at(scan_idx, BLACK) != EMPTY) {
                    
                    // If the piece we hit belongs to the enemy, is it a Pontiff?
                    if (board.get_piece_at(scan_idx, attacker_color) == PONTIFF) {
                        return true; // We are in check!
                    }
                    
                    // If it's anything else (friendly piece, enemy pawn, etc.), the ray is blocked.
                    break; 
                }
                
                cx = nx;
                cy = ny;
            }
        }

        return false;
    }

    // 118. Royal Piece Check Detection (Extensible for multiple Royals!)
    inline bool is_in_check(const BoardState& board, const BoardGeometry& geo, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;

        // --- DEFINE YOUR ROYAL PIECES HERE ---
        // Right now it's just the KING. 
        // If the REGENT becomes royal, just change this to:
        // Bitboard192 royals = board.pieces[us][KING] | board.pieces[us][REGENT];
        Bitboard192 royals = board.pieces[us][KING];

        // Check if ANY royal piece is currently under attack
        while (royals.popcount() > 0) {
            int royal_idx = royals.lsb();
            royals.clear_bit(royal_idx);
            
            if (is_square_attacked(board, geo, royal_idx, them)) {
                return true; 
            }
        }
        return false;
    }

    // 108 & 109. Pawn Pushes, Captures, and En Passant
    inline void generate_pawn_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        Color them = (us == WHITE) ? BLACK : WHITE;
        
        // White is at bottom (Row 6) -> moves -12
        // Black is at top (Row 1) -> moves +12
        int forward = (us == WHITE) ? -12 : 12; 
        
        Bitboard192 pawns = board.pieces[us][PAWN];
        
        while (pawns.popcount() > 0) {
            int from = pawns.lsb();
            pawns.clear_bit(from); 
            
            auto [x, y] = geo.index_to_coord(from);
            
            // 1. Single Push
            int push_to = from + forward;
            if (geo.is_in_bounds(push_to) && 
                !board.occupancy[WHITE].test_bit(push_to) && 
                !board.occupancy[BLACK].test_bit(push_to)) {
                
                auto [px, py] = geo.index_to_coord(push_to);
                bool promotes = (us == WHITE && py == 0) || (us == BLACK && py == geo.active_height - 1);

                if (promotes) {
                    // Generate all 4 promotion options
                    moves.add(Move(from, push_to, QUEEN, EMPTY, FLAG_PROMOTION));
                    moves.add(Move(from, push_to, ROOK, EMPTY, FLAG_PROMOTION));
                    moves.add(Move(from, push_to, BISHOP, EMPTY, FLAG_PROMOTION));
                    moves.add(Move(from, push_to, KNIGHT, EMPTY, FLAG_PROMOTION));
                } else {
                    moves.add(Move(from, push_to, PAWN));
                    
                    // 2. THE DOUBLE PUSH
                    if ((us == WHITE && y == 6) || (us == BLACK && y == 1)) {
                        int double_push = push_to + forward;
                        
                        if (geo.is_in_bounds(double_push) && 
                            !board.occupancy[WHITE].test_bit(double_push) && 
                            !board.occupancy[BLACK].test_bit(double_push)) {
                            
                            moves.add(Move(from, double_push, PAWN));
                        }
                    }
                }
            }
            
            // 3. Diagonal Captures & En Passant
            int caps[2] = { from + forward - 1, from + forward + 1 };
            for (int to : caps) {
                if (geo.is_in_bounds(to)) {
                    auto [to_x, to_y] = geo.index_to_coord(to);
                    if (std::abs(to_x - x) == 1) { 
                        
                        bool promotes = (us == WHITE && to_y == 0) || (us == BLACK && to_y == geo.active_height - 1);

                        // Standard Capture
                        if (board.occupancy[them].test_bit(to)) {
                            if (!is_jester(board, them, to)) { 
                                PieceID cap_piece = board.get_piece_at(to, them);
                                
                                if (promotes) {
                                    moves.add(Move(from, to, QUEEN, cap_piece, FLAG_PROMOTION));
                                    moves.add(Move(from, to, ROOK, cap_piece, FLAG_PROMOTION));
                                    moves.add(Move(from, to, BISHOP, cap_piece, FLAG_PROMOTION));
                                    moves.add(Move(from, to, KNIGHT, cap_piece, FLAG_PROMOTION));
                                } else {
                                    moves.add(Move(from, to, PAWN, cap_piece, FLAG_CAPTURE));
                                }
                            }
                        }
                        // NEW: En Passant Capture
                        else if (to == board.ep_target_index) {
                            moves.add(Move(from, to, PAWN, PAWN, FLAG_EN_PASSANT));
                        }
                        
                    }
                }
            }
        }
    }

    // 113. LUT-based Stepping Moves (Now with CASTLING!)
    inline void generate_step_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us, PieceID piece, const Bitboard192* lut) {
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

            // --- CASTLING LOGIC ---
            if (piece == KING) {
                // If King is currently in check, Castling is strictly illegal
                if (is_square_attacked(board, geo, from, them)) continue;

                // Grab the current rights for the active color
                bool can_kingside = board.castling_rights & ((us == WHITE) ? WHITE_OO : BLACK_OO);
                bool can_queenside = board.castling_rights & ((us == WHITE) ? WHITE_OOO : BLACK_OOO);

                // --- Kingside Castling (O-O) ---
                if (can_kingside) {
                    int k_rook = from + 3; // Rook is 3 squares to the right
                    int f_sq = from + 1;
                    int g_sq = from + 2;

                    // 1. Are the squares empty?
                    if (!board.occupancy[NONE].test_bit(f_sq) && !board.occupancy[NONE].test_bit(g_sq)) {
                        // 2. Are the squares safe from attack?
                        if (!is_square_attacked(board, geo, f_sq, them) && !is_square_attacked(board, geo, g_sq, them)) {
                            moves.add(Move(from, g_sq, KING, EMPTY, FLAG_CASTLE));
                        }
                    }
                }

                // --- Queenside Castling (O-O-O) ---
                if (can_queenside) {
                    int q_rook = from - 4; // Rook is 4 squares to the left
                    int d_sq = from - 1;
                    int c_sq = from - 2;
                    int b_sq = from - 3;

                    // 1. Are the squares empty? (Note: b_sq doesn't need to be checked for attacks, just empty)
                    if (!board.occupancy[NONE].test_bit(d_sq) && !board.occupancy[NONE].test_bit(c_sq) && !board.occupancy[NONE].test_bit(b_sq)) {
                        // 2. Are the squares safe from attack?
                        if (!is_square_attacked(board, geo, d_sq, them) && !is_square_attacked(board, geo, c_sq, them)) {
                            moves.add(Move(from, c_sq, KING, EMPTY, FLAG_CASTLE));
                        }
                    }
                }
            }
        }
    }

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

    inline void generate_pontiff_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color color) {
        int dirs[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        Bitboard192 pontiffs = board.pieces[color][PONTIFF];
        Color enemy_color = (color == WHITE) ? BLACK : WHITE;

        while (pontiffs.popcount() > 0) {
            int sq = pontiffs.lsb(); pontiffs.clear_bit(sq);
            bool visited[144] = {false}; 

            for (int d = 0; d < 4; ++d) {
                int cx = geo.index_to_coord(sq).first;
                int cy = geo.index_to_coord(sq).second;
                int dx = dirs[d][0];
                int dy = dirs[d][1];

                for (int step = 0; step < 25; ++step) {
                    int nx = cx + dx;
                    int ny = cy + dy;

                    if (nx < 0 || nx >= geo.active_width) {
                        dx = -dx;
                        nx = cx + dx;
                    }
                    if (ny < 0 || ny >= geo.active_height) {
                        dy = -dy;
                        ny = cy + dy;
                    }

                    int target_idx = geo.coord_to_index(nx, ny);
                    if (target_idx == sq) break; 

                    PieceID my_piece = board.get_piece_at(target_idx, color);
                    if (my_piece != EMPTY) break; 

                    PieceID enemy_piece = board.get_piece_at(target_idx, enemy_color);

                    if (!visited[target_idx]) {
                        visited[target_idx] = true;
                        
                        // --- THE FIX: Apply FLAG_CAPTURE if an enemy is hit ---
                        MoveFlag flag = (enemy_piece != EMPTY) ? FLAG_CAPTURE : FLAG_NONE;
                        
                        Move new_move(sq, target_idx, PONTIFF, enemy_piece, flag);
                        moves.moves.push_back(new_move);
                        moves.macro_chains.push_back(std::vector<int>());
                    }

                    if (enemy_piece != EMPTY) break; 
                    cx = nx;
                    cy = ny;
                }
            }
        }
    }

    // The True Custom Peasant: Walks Diagonal (RR/LL dash), Captures Straight
    inline void generate_peasant_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color color) {
        Bitboard192 peasants = board.pieces[color][PEASANT];
        int forward = (color == WHITE) ? -1 : 1;
        int start_rank = (color == WHITE) ? 6 : 1;
        int prom_rank = (color == WHITE) ? 0 : geo.active_height - 1;

        while (peasants.popcount() > 0) {
            int sq = peasants.lsb(); peasants.clear_bit(sq);
            auto [cx, cy] = geo.index_to_coord(sq);

            // 1. WALK DIAGONALLY (No captures)
            int walk_dirs[2] = {-1, 1}; // Left-diagonal and Right-diagonal
            for (int dx : walk_dirs) {
                int nx = cx + dx;
                int ny = cy + forward;
                
                if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                    int target_idx = geo.coord_to_index(nx, ny);
                    
                    // If square is empty, we can walk there
                    if (board.get_piece_at(target_idx, WHITE) == EMPTY && board.get_piece_at(target_idx, BLACK) == EMPTY) {
                        
                        MoveFlag flag = (ny == prom_rank) ? FLAG_PROMOTION : FLAG_NONE;
                        moves.moves.push_back(Move(sq, target_idx, PEASANT, EMPTY, flag));
                        moves.macro_chains.push_back(std::vector<int>());

                        // DOUBLE DIAGONAL DASH (RR or LL)
                        // Must be from start rank, must continue in the exact same dx direction
                        if (cy == start_rank) {
                            int nnx = nx + dx; // Keep going left (LL) or right (RR)
                            int nny = ny + forward;
                            
                            if (nnx >= 0 && nnx < geo.active_width && nny >= 0 && nny < geo.active_height) {
                                int double_idx = geo.coord_to_index(nnx, nny);
                                
                                // Intermediate square (target_idx) is already verified empty. 
                                // Now check if the final landing square is empty!
                                if (board.get_piece_at(double_idx, WHITE) == EMPTY && board.get_piece_at(double_idx, BLACK) == EMPTY) {
                                    moves.moves.push_back(Move(sq, double_idx, PEASANT, EMPTY, FLAG_NONE));
                                    moves.macro_chains.push_back(std::vector<int>());
                                }
                            }
                        }
                    }
                }
            }

            // 2. CAPTURE STRAIGHT FORWARD
            int eat_x = cx;
            int eat_y = cy + forward;
            if (eat_y >= 0 && eat_y < geo.active_height) {
                int eat_idx = geo.coord_to_index(eat_x, eat_y);
                Color enemy = (color == WHITE) ? BLACK : WHITE;
                PieceID enemy_piece = board.get_piece_at(eat_idx, enemy);
                
                // If there is an enemy directly in front, eat it!
                if (enemy_piece != EMPTY) {
                    MoveFlag flag = (eat_y == prom_rank) ? FLAG_PROMOTION : FLAG_CAPTURE;
                    moves.moves.push_back(Move(sq, eat_idx, PEASANT, enemy_piece, flag));
                    moves.macro_chains.push_back(std::vector<int>());
                }
                
                // EN PASSANT CAPTURE
                // If the square straight ahead is the En Passant square (the square the enemy skipped)
                // Note: Change `ep_square` to whatever your variable is actually named!
                /*
                if (eat_idx == board.ep_square) {
                    moves.moves.push_back(Move(sq, eat_idx, PEASANT, PEASANT, FLAG_EN_PASSANT));
                    moves.macro_chains.push_back(std::vector<int>());
                }
                */
            }
        }
    }

    // 117. Master Pseudo-Legal Generator
    inline void generate_pseudo_legal_moves(const BoardState& board, const BoardGeometry& geo, MoveList& moves, Color us) {
        generate_pawn_moves(board, geo, moves, us);
        generate_step_moves(board, geo, moves, us, KNIGHT, geo.knight_attacks);
        generate_step_moves(board, geo, moves, us, KING, geo.king_attacks);
        
        // --- STANDARD SLIDERS ---
        generate_sliding_moves(board, geo, moves, us, ROOK, SLIDE_ORTHOGONAL, 99);
        generate_sliding_moves(board, geo, moves, us, BISHOP, SLIDE_DIAGONAL, 99);
        generate_sliding_moves(board, geo, moves, us, QUEEN, SLIDE_ALL, 99);

        // --- Custom Pieces ---
        generate_step_moves(board, geo, moves, us, REGENT, geo.king_attacks);
        generate_sliding_moves(board, geo, moves, us, TOWER, SLIDE_ORTHOGONAL, 99);
        generate_harpooner_moves(board, geo, moves, us);
        generate_culverin_moves(board, geo, moves, us);
        generate_keg_moves(board, geo, moves, us); 
        generate_bandit_moves(board, geo, moves, us); 
        generate_pontiff_moves(board, geo,moves,us);
        generate_step_moves(board, geo, moves, us, SOLDIER, geo.king_attacks); 
        generate_peasant_moves(board, geo, moves, us);

    }

    // 115. Filter Safe King (Now using Extensible Royal Logic)
    inline void generate_legal_moves(const BoardState& board, const BoardGeometry& geo, MoveList& legal_moves, Color us) {
        MoveList pseudo;
        generate_pseudo_legal_moves(board, geo, pseudo, us);

        Color them = (us == WHITE) ? BLACK : WHITE;

        for (size_t i = 0; i < pseudo.size(); i++) {
            Move m = pseudo.moves[i];
            
            // --- THE SILVER BULLET ---
            // The King is never actually captured in chess. 
            // If a move tries to capture a King, it is strictly illegal.
            if (m.get_captured() == KING) continue;
            
            // --- CASTLING FAST-TRACK ---
            // Castling is rigorously safety-checked inside generate_step_moves.
            // Bypassing the simulation saves CPU and prevents missing-rook bugs!
            if (m.get_flag() == FLAG_CASTLE) {
                legal_moves.add(m);
                continue;
            }

            BoardState test_board = board;
            
            // --- EN PASSANT GHOST FIX ---
            if (m.get_flag() == FLAG_CAPTURE) {
                test_board.remove_piece(them, m.get_captured(), m.get_to());
            } 
            else if (m.get_flag() == FLAG_EN_PASSANT) {
                int backward = (us == WHITE) ? geo.active_width : -geo.active_width;
                test_board.remove_piece(them, m.get_captured(), m.get_to() + backward);
            }

            // Simulate the physical move
            test_board.move_piece(us, m.get_piece(), m.get_from(), m.get_to());

            // --- THE ROYAL UPGRADE ---
            // Instead of searching for the KING specifically, we just ask: 
            // "Is ANY royal piece in check right now?"
            if (!is_in_check(test_board, geo, us)) {
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