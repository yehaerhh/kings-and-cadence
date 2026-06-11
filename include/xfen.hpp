#pragma once
#include "state.hpp"
#include "geometry.hpp"
#include <string>
#include <unordered_map>
#include <cctype>

namespace Engine::XFEN {

    // 91 & 92 & 96: Token Maps for standard and bracketed pieces
    inline std::unordered_map<std::string, std::pair<Color, PieceID>> token_map = {
        {"K", {WHITE, KING}}, {"Q", {WHITE, QUEEN}}, {"R", {WHITE, ROOK}}, {"B", {WHITE, BISHOP}}, {"N", {WHITE, KNIGHT}}, {"P", {WHITE, PAWN}},
        {"k", {BLACK, KING}}, {"q", {BLACK, QUEEN}}, {"r", {BLACK, ROOK}}, {"b", {BLACK, BISHOP}}, {"n", {BLACK, KNIGHT}}, {"p", {BLACK, PAWN}},
        {"[Pe]", {WHITE, PEASANT}}, {"[pe]", {BLACK, PEASANT}},
        {"[Ba]", {WHITE, BANDIT}},  {"[ba]", {BLACK, BANDIT}},
        {"[Cu]", {WHITE, CULVERIN}},{"[cu]", {BLACK, CULVERIN}},
        {"[Ha]", {WHITE, HARPOONER}},{"[ha]", {BLACK, HARPOONER}},
        {"[Ke]", {WHITE, POWDER_KEG}},{"[ke]", {BLACK, POWDER_KEG}},
        {"[To]", {WHITE, TOWER}},   {"[to]", {BLACK, TOWER}},
        {"[Re]", {WHITE, REGENT}},  {"[re]", {BLACK, REGENT}},
        {"[Je]", {WHITE, JESTER}},  {"[je]", {BLACK, JESTER}}
    };

    // 94: Parse the placement data block
    inline void load_fen(BoardState& board, const BoardGeometry& geo, const std::string& fen) {
        board.clear();
        int x = 0, y = geo.active_height - 1; // FENs start at the top-left (e.g., Rank 8 or 12)
        
        for (size_t i = 0; i < fen.length(); ++i) {
            char c = fen[i];
            if (c == ' ') break; // End of board placement segment
            
            if (c == '/') { // Move to next row down
                y--; x = 0;
                continue;
            }
            
            // 95. Bracketed Custom Piece Scanner
            if (c == '[') {
                std::string token = "";
                while (i < fen.length() && fen[i] != ']') {
                    token += fen[i];
                    i++;
                }
                token += ']'; 
                
                if (token_map.count(token)) {
                    auto [color, piece] = token_map[token];
                    board.add_piece(color, piece, geo.coord_to_index(x, y));
                }
                x++;
            } 
            // Standard pieces or empty space numbers
            else {
                if (std::isdigit(c)) {
                    // 97. Support multi-digit numbers for N x N boards
                    int empty_squares = c - '0';
                    while (i + 1 < fen.length() && std::isdigit(fen[i+1])) {
                        empty_squares = empty_squares * 10 + (fen[i+1] - '0');
                        i++;
                    }
                    x += empty_squares;
                } else {
                    std::string token(1, c);
                    if (token_map.count(token)) {
                        auto [color, piece] = token_map[token];
                        board.add_piece(color, piece, geo.coord_to_index(x, y));
                    }
                    x++;
                }
            }
        }
    }

    // 101. Inverse function: BoardState -> X-FEN String
    inline std::string generate_xfen(const BoardState& board, const BoardGeometry& geo) {
        std::string fen = "";            
        // Start from the top row and read downwards, just like standard FEN
        for (int y = geo.active_height - 1; y >= 0; --y) {
            int empty_count = 0;
            for (int x = 0; x < geo.active_width; ++x) {
                int index = geo.coord_to_index(x, y);
                    
                PieceID w_piece = board.get_piece_at(index, WHITE);
                PieceID b_piece = board.get_piece_at(index, BLACK);
                    
                if (w_piece == EMPTY && b_piece == EMPTY) {
                    empty_count++;
                } else {
                    // We hit a piece, append any accumulated empty squares first
                    if (empty_count > 0) {
                        fen += std::to_string(empty_count);
                        empty_count = 0;
                    }
                        
                    Color c = (w_piece != EMPTY) ? WHITE : BLACK;
                    PieceID p = (w_piece != EMPTY) ? w_piece : b_piece;
                        
                    // Reverse lookup: Find the string token for this exact piece
                    for (const auto& [token, pair] : token_map) {
                        if (pair.first == c && pair.second == p) {
                            fen += token;
                            break;
                        }
                    }
                }
            }
            // Append any remaining empty squares at the end of the row
            if (empty_count > 0) fen += std::to_string(empty_count);
                
            // Add the row separator, except for the very last row
            if (y > 0) fen += "/";
        }
        return fen;
    }
}   