#pragma once
#include "bitboard.hpp"
#include "types.hpp"
#include "zobrist.hpp"
#include "move.hpp"
#include "geometry.hpp"
#include <vector>
#include <iostream>
#include <cctype>

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

        // Add this diagnostic function inside BoardState
        // Upgraded Diagnostic Printer
        void print_board(const BoardGeometry& geo) const {
            std::cout << "\n--- C++ INTERNAL BOARD STATE ---" << std::endl;
            for (int y = 0; y < geo.active_height; ++y) {
                // Print the Y-axis coordinate for readability
                if (y < 10) std::cout << "Row  " << y << " | ";
                else std::cout << "Row " << y << " | ";
                
                for (int x = 0; x < geo.active_width; ++x) {
                    int idx = geo.coord_to_index(x, y);
                    
                    if (occupancy[WHITE].test_bit(idx)) {
                        PieceID p = get_piece_at(idx, WHITE);
                        char c = piece_to_char(p);
                        std::cout << c << " ";
                    } else if (occupancy[BLACK].test_bit(idx)) {
                        PieceID p = get_piece_at(idx, BLACK);
                        char c = std::tolower(piece_to_char(p));
                        std::cout << c << " ";
                    } else {
                        std::cout << ". ";
                    }
                }
                std::cout << std::endl;
            }
            std::cout << "--------------------------------\n" << std::endl;
        }


        // Helper to map PieceID to a visual character
        // Helper to map PieceID to a visual character
        char piece_to_char(PieceID p) const {
            switch(p) {
                case PAWN: return 'P';
                case KNIGHT: return 'N';
                case BISHOP: return 'B';
                case ROOK: return 'R';
                case QUEEN: return 'Q';
                case KING: return 'K';
                
                // --- Custom Mechanics ---
                case BANDIT: return 'D';       // D for Dash/Bandit
                case CULVERIN: return 'C';
                case HARPOONER: return 'H';
                case POWDER_KEG: return 'X';   // X for Explosive
                case TOWER: return 'T';
                case REGENT: return 'G';       // G for reGent
                case JESTER: return 'J';
                
                // --- The New Vanguard ---
                case PONTIFF: return 'F';      // F for pontiFf (since P is Pawn)
                case PEASANT: return 'E';      // E for pEasant
                case SOLDIER: return 'S';      // S for Soldier
                
                default: return '?';
            }
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

        // 124, 129 & 149+. The Master Move Compiler
        inline void execute_move(const Move& m, const BoardGeometry& geo, const std::vector<int>& macro_chain = {}) {
            Color us = turn;
            Color them = (us == WHITE) ? BLACK : WHITE;

            int from = m.get_from();
            int to = m.get_to();
            PieceID piece = m.get_piece();
            PieceID captured = m.get_captured();
            MoveFlag flag = m.get_flag();

            history.push_back({hash_key, half_move_clock, castling_rights, ep_target_index, captured});
            ep_target_index = -1; 

            PieceID moving_piece = (flag == FLAG_PROMOTION) ? PEASANT : piece;

            // 1. Remove the moving piece from its origin
            remove_piece(us, moving_piece, from);

            // 2. Handle Complex Effects
            if (flag == FLAG_DISPLACE) {
                auto [fx, fy] = geo.index_to_coord(from);
                auto [tx, ty] = geo.index_to_coord(to);
                int dx = (fx > tx) ? 1 : (fx < tx) ? -1 : 0;
                int dy = (fy > ty) ? 1 : (fy < ty) ? -1 : 0;
                int landing_idx = geo.coord_to_index(tx + (dx * 2), ty + (dy * 2));
                remove_piece(them, captured, to);
                add_piece(them, captured, landing_idx);
                add_piece(us, piece, from); 
            }
            else if (flag == FLAG_SPLASH) {
                if (piece == CULVERIN) {
                    remove_piece(them, captured, to); 
                    add_piece(us, piece, from); 
                }
                else if (piece == POWDER_KEG) {
                    auto [tx, ty] = geo.index_to_coord(to);
                    for (int x = -1; x <= 1; ++x) {
                        for (int y = -1; y <= 1; ++y) {
                            int nx = tx + x, ny = ty + y;
                            if (nx >= 0 && nx < geo.active_width && ny >= 0 && ny < geo.active_height) {
                                int blast_idx = geo.coord_to_index(nx, ny);
                                PieceID w_piece = get_piece_at(blast_idx, WHITE);
                                PieceID b_piece = get_piece_at(blast_idx, BLACK);
                                if (w_piece != EMPTY) remove_piece(WHITE, w_piece, blast_idx);
                                if (b_piece != EMPTY) remove_piece(BLACK, b_piece, blast_idx);
                            }
                        }
                    }
                }
            }
            else if (flag == FLAG_CHAIN_DASH) {
                for (int target_idx : macro_chain) {
                    PieceID cap = get_piece_at(target_idx, them);
                    if (cap != EMPTY) {
                        remove_piece(them, cap, target_idx);
                    }
                }
                add_piece(us, piece, to); 
            }
            else if (flag == FLAG_EN_PASSANT) {
                add_piece(us, piece, to);
                // THE FIX: Dynamic backward stride for removing the captured piece
                int backward = (us == WHITE) ? geo.active_width : -geo.active_width; 
                remove_piece(them, captured, to + backward);
            }
            else if (flag == FLAG_CASTLE) {
                // The King lands on the 'to' square normally
                add_piece(us, KING, to);

                // Teleport the Rook depending on the direction!
                if (to > from) { 
                    // Kingside (to = from + 2)
                    int rook_origin = from + 3;
                    int rook_dest = from + 1;
                    remove_piece(us, ROOK, rook_origin);
                    add_piece(us, ROOK, rook_dest);
                } else {
                    // Queenside (to = from - 2)
                    int rook_origin = from - 4;
                    int rook_dest = from - 1;
                    remove_piece(us, ROOK, rook_origin);
                    add_piece(us, ROOK, rook_dest);
                }
            }
            else {
                if (captured != EMPTY) {
                    remove_piece(them, captured, to);
                }
                add_piece(us, piece, to);

                // THE FIX: Dynamic stride check for creating the En Passant target
                if (piece == PEASANT && std::abs(to - from) == (geo.active_width * 2)) { 
                    int backward = (us == WHITE) ? geo.active_width : -geo.active_width; 
                    ep_target_index = to + backward; 
                }
            }

            // 3. --- CASTLING RIGHTS DESTRUCTION ---
            // If the rights are already 0, skip this logic to save CPU time
            if (castling_rights != CASTLE_NONE) {
                // THE FIX: Dynamically track White's back row based on active_height
                int w_king = geo.coord_to_index(4, geo.active_height - 1);
                int w_rook_k = geo.coord_to_index(7, geo.active_height - 1);
                int w_rook_q = geo.coord_to_index(0, geo.active_height - 1);
                
                int b_king = geo.coord_to_index(4, 0);
                int b_rook_k = geo.coord_to_index(7, 0);
                int b_rook_q = geo.coord_to_index(0, 0);

                // If King moves (or is captured), destroy BOTH rights for that color
                if (from == w_king || to == w_king) castling_rights &= ~(WHITE_OO | WHITE_OOO);
                if (from == b_king || to == b_king) castling_rights &= ~(BLACK_OO | BLACK_OOO);

                // If a Rook moves (or is captured), destroy only THAT specific right
                if (from == w_rook_k || to == w_rook_k) castling_rights &= ~WHITE_OO;
                if (from == w_rook_q || to == w_rook_q) castling_rights &= ~WHITE_OOO;
                if (from == b_rook_k || to == b_rook_k) castling_rights &= ~BLACK_OO;
                if (from == b_rook_q || to == b_rook_q) castling_rights &= ~BLACK_OOO;
            }

            // 4. Swap Turn
            if (flag != FLAG_CHAIN_DASH) {
                HashEngine::toggle_turn(hash_key);
                turn = them;
            }
        }
    };

} // namespace Engine