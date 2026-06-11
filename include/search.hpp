#pragma once
#include "movegen.hpp"
#include "state.hpp"
#include "tt.hpp"
#include "eval.hpp"
#include <iostream>

namespace Engine::Search {

    const int INF = 30000;
    const int MATE = 29000;

    // 178. Quiescence Search (Resolves the Horizon Effect)
    inline int qsearch(BoardState& board, const BoardGeometry& geo, int alpha, int beta, int q_depth=0) {
        if(q_depth > 15) return Eval::evaluate(board, geo);
        
        int stand_pat = Eval::evaluate(board, geo);
        
        if (stand_pat >= beta) {
            return beta;
        }
        if (alpha < stand_pat) {
            alpha = stand_pat;
        }

        MoveList tactical_moves;
        MoveGen::generate_tactical_moves(board, geo, tactical_moves, board.turn);

        for (size_t i = 0; i < tactical_moves.size(); ++i) {
            const auto& m = tactical_moves.moves[i];
            BoardState backup = board;
            
            board.execute_move(m, geo, tactical_moves.macro_chains[i]);
            
            // --- ARCHITECTURE FIX 1: Q-Search Turn Flip ---
            // NegaMax calculates from the perspective of the side to move. 
            // Because execute_move only updates bitboards and not board.turn, 
            // we MUST manually flip the turn here so the recursive call knows 
            // it is evaluating the opponent's options.
            board.turn = (board.turn == WHITE) ? BLACK : WHITE; 
            
            int score = -qsearch(board, geo, -beta, -alpha, q_depth+1);
            
            board = backup; // Auto-reverts the turn back to us

            if (score >= beta) return beta; 
            if (score > alpha) alpha = score; 
        }

        return alpha;
    }

    // 167. Core PVS Loop with Transposition Table integration
    inline int pvs(BoardState& board, const BoardGeometry& geo, TranspositionTable& tt, int depth, int alpha, int beta) {
        uint64_t current_hash = board.hash_key; 
        int16_t cached_score;
        EntryType cached_type;
        Move cached_move;
        
        if (tt.probe(current_hash, depth, cached_score, cached_type, cached_move)) {
            if (cached_type == EXACT) return cached_score;
            if (cached_type == LOWERBOUND && cached_score >= beta) return beta;
            if (cached_type == UPPERBOUND && cached_score <= alpha) return alpha;
        }
        
        if (depth == 0) return qsearch(board, geo, alpha, beta); 

        MoveList moves;
        MoveGen::generate_legal_moves(board, geo, moves, board.turn);

        if (moves.size() == 0) return 0; 

        Move best_move;
        EntryType type = UPPERBOUND; 
        bool first_move = true;

        for (size_t i = 0; i < moves.size(); ++i) {
            const auto& m = moves.moves[i];
            BoardState backup = board;
            
            board.execute_move(m, geo, moves.macro_chains[i]);
            
            // --- ARCHITECTURE FIX 2: Main PVS Turn Flip ---
            // Just like Q-Search, this prevents the sign-inversion bug that 
            // was making the AI think developing major pieces was a negative play.
            board.turn = (board.turn == WHITE) ? BLACK : WHITE;
            
            int score;
            if (first_move) {
                score = -pvs(board, geo, tt, depth - 1, -beta, -alpha);
            } else {
                score = -pvs(board, geo, tt, depth - 1, -alpha - 1, -alpha);
                if (score > alpha && score < beta) {
                    score = -pvs(board, geo, tt, depth - 1, -beta, -alpha);
                }
            }
            
            board = backup; // Auto-reverts the turn back to us
            
            if (score >= beta) {
                //tt.store(current_hash, depth, beta, LOWERBOUND, m);
                return beta; 
            }
            if (score > alpha) {
                alpha = score;
                best_move = m; 
                type = EXACT; 
            }
            first_move = false;
        }
        
        //tt.store(current_hash, depth, alpha, type, best_move);
        return alpha;
    }

    // 171. Iterative Deepening & AI Brain Logger
    inline Move get_best_move(BoardState& board, const BoardGeometry& geo, TranspositionTable& tt, int max_depth) {
        Move absolute_best_move;
        
        std::cout << "\n==================================================" << std::endl;
        std::cout << "🧠 [AI BRAIN LOG] Initializing Iterative Deepening..." << std::endl;
        std::cout << "==================================================" << std::endl;

        for (int d = 1; d <= max_depth; ++d) {
            std::cout << "\n--- SEARCHING DEPTH " << d << " ---" << std::endl;
            
            // --- ARCHITECTURE UPDATE 3: Unrolling the Root Node ---
            // In your original code, you just called pvs() here and let the TT remember the move.
            // By "unrolling" the first layer of the tree right here, we can intercept the math
            // and use std::cout to print the exact score of every single legal root move 
            // before it dives into the PVS recursion.
            
            MoveList root_moves;
            MoveGen::generate_legal_moves(board, geo, root_moves, board.turn);
            
            int alpha = -INF;
            int beta = INF;
            Move best_move_this_depth;

            for (size_t i = 0; i < root_moves.size(); ++i) {
                const auto& m = root_moves.moves[i];
                BoardState backup = board;
                
                board.execute_move(m, geo, root_moves.macro_chains[i]);
                board.turn = (board.turn == WHITE) ? BLACK : WHITE; 
                
                int score = -pvs(board, geo, tt, d - 1, -beta, -alpha);
                
                board = backup; 
                
                auto [fx, fy] = geo.index_to_coord(m.get_from());
                auto [tx, ty] = geo.index_to_coord(m.get_to());
                std::cout << "   Evaluating: (" << fx << "," << fy << ") -> (" << tx << "," << ty << ") | Score: " << score << std::endl;
                
                if (score > alpha) {
                    alpha = score;
                    best_move_this_depth = m;
                }
            }
            
            auto [bx, by] = geo.index_to_coord(best_move_this_depth.get_from());
            auto [btx, bty] = geo.index_to_coord(best_move_this_depth.get_to());
            std::cout << "🏆 BEST MOVE AT DEPTH " << d << ": (" << bx << "," << by << ") -> (" << btx << "," << bty << ") | Final Eval: " << alpha << std::endl;
            
            absolute_best_move = best_move_this_depth;
        }
        
        std::cout << "==================================================\n" << std::endl;
        return absolute_best_move;
    }
}