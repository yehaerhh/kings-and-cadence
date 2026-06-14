#pragma once
#include "movegen.hpp"
#include "state.hpp"
#include "tt.hpp"
#include "eval.hpp"
#include <iostream>
#include <chrono> 
#include <algorithm> // Required for std::sort
#include <vector>

namespace Engine::Search {

    const int INF = 30000;
    const int MATE = 29000;

    

    // --- TASKS 1 & 2: PIECE VALUE MAPPING ---
    inline int get_piece_value(PieceID piece) {
        switch (piece) {
            case PAWN:        return 100;
            case PEASANT:     return 100; 
            case KNIGHT:      return 300;
            case BISHOP:      return 300;
            case BANDIT:      return 350; // Agile fantasy skirmisher
            case JESTER:      return 400; // Chaotic utility piece
            case HARPOONER:   return 450; // Positional pull/displacement piece
            case ROOK:        return 500;
            case CULVERIN:    return 550; // Long-range anti-piece artillery
            case POWDER_KEG:  return 600; // High-value volatile hazard
            case TOWER:       return 700; // Upgraded heavy rook structure
            case REGENT:      return 850; // Prime royal protector
            case QUEEN:       return 900;
            case KING:        return 10000;
            default:          return 0;
        }
    }

    // Helper: Use your existing BoardState logic to find the victim!
    inline PieceID get_victim_piece(const BoardState& board, int target_square) {
        Color opp = (board.turn == WHITE) ? BLACK : WHITE;
        return board.get_piece_at(target_square, opp);
    }

    inline Move killer_moves[64][2];

    // Core Move Scorer utilizing Generalized MVV-LVA + TT Move Injection
    inline int score_move(const Move& m, const BoardState& board, const Move& tt_move, int depth) {
        // --- TASK 3: TT MOVE INJECTION ---
        // If this move matches the exact move the TT recommended, search it ABSOLUTELY FIRST.
        if (tt_move.get_piece() != EMPTY) {
            if (m.get_from() == tt_move.get_from() && m.get_to() == tt_move.get_to()) {
                return 2000000; // Unbeatable priority score
            }
        }

        // --- TASK 2: MVV-LVA ---
        PieceID attacker = m.get_piece();
        PieceID victim = get_victim_piece(board, m.get_to());

        if (victim != EMPTY) {
            return 100000 + get_piece_value(victim) - (get_piece_value(attacker) / 100);
        }

        // --- TASK 4: KILLER MOVE HEURISTIC ---
        // If it's a quiet move (no capture), check if it's a known killer for this depth
        if (depth >= 0 && depth < 64) {
            if (m.get_from() == killer_moves[depth][0].get_from() && m.get_to() == killer_moves[depth][0].get_to()) {
                return 90000; // 1st Killer (Searched right after captures)
            }
            if (m.get_from() == killer_moves[depth][1].get_from() && m.get_to() == killer_moves[depth][1].get_to()) {
                return 80000; // 2nd Killer
            }
        }

        // Quiet moves default to 0
        return 0;
    }

    // TASK 5: SYNCHRONIZED MOVE SORTING (Index-Based)
    inline void sort_moves(MoveList& moves, const BoardState& board, const Move& tt_move = Move(), int depth =0) {
        if (moves.size() <= 1) return;

        std::vector<int> scores(moves.size());
        std::vector<size_t> indices(moves.size());

        for (size_t i = 0; i < moves.size(); ++i) {
            // Pass the TT move down into the scorer
            scores[i] = score_move(moves.moves[i], board, tt_move, depth);
            indices[i] = i;
        }

        std::sort(indices.begin(), indices.end(), [&scores](size_t a, size_t b) {
            return scores[a] > scores[b];
        });

        std::vector<Move> temp_moves(moves.size());
        std::vector<std::vector<int>> temp_chains(moves.size());

        for (size_t i = 0; i < moves.size(); ++i) {
            temp_moves[i] = moves.moves[indices[i]];
            temp_chains[i] = moves.macro_chains[indices[i]];
        }

        for (size_t i = 0; i < moves.size(); ++i) {
            moves.moves[i] = temp_moves[i];
            moves.macro_chains[i] = temp_chains[i];
        }
    }

    // Quiescence Search with prioritized move lists
    inline int qsearch(BoardState& board, const BoardGeometry& geo, int alpha, int beta, long long& node_count, int q_depth=0) {
        node_count++;
        if(q_depth > 15) return Eval::evaluate(board, geo);
        
        int stand_pat = Eval::evaluate(board, geo);
        
        if (stand_pat >= beta) return beta;
        if (alpha < stand_pat) alpha = stand_pat;

        MoveList tactical_moves;
        MoveGen::generate_tactical_moves(board, geo, tactical_moves, board.turn);
        
        // Sort tactical options to handle the most violent threats first
        sort_moves(tactical_moves, board);

        for (size_t i = 0; i < tactical_moves.size(); ++i) {
            const auto& m = tactical_moves.moves[i];
            BoardState backup = board;
            
            board.execute_move(m, geo, tactical_moves.macro_chains[i]);
            
            int score;
            if (m.get_flag() == FLAG_CHAIN_DASH) {
                score = qsearch(board, geo, alpha, beta, node_count, q_depth+1);
            } else {
                score = -qsearch(board, geo, -beta, -alpha, node_count, q_depth+1);
            }
            
            board = backup; 

            if (score >= beta) return beta; 
            if (score > alpha) alpha = score; 
        }

        return alpha;
    }

    // Core PVS Loop with full TT implementation, move sorting, NMP, RFP, and LMR
    inline int pvs(BoardState& board, const BoardGeometry& geo, TranspositionTable& tt, int depth, int alpha, int beta, long long& node_count) {
        node_count++;

        // 1. TRANSPOSITION TABLE PROBE
        int16_t tt_score;
        EntryType tt_type;
        Move tt_move = Move(); // Initialize safely
        
        if (tt.probe(board.hash_key, depth, tt_score, tt_type, tt_move)) {
            // --- THE FIX: TT MATE BYPASS ---
            // If the TT score is a mate score, do NOT return it as an exact cutoff!
            // We force the engine to actually calculate the mate at this depth 
            // so it doesn't hallucinate a mate from a shallower search.
            bool is_mate_score = (tt_score > 28000 || tt_score < -28000);
            
            if (!is_mate_score) {
                if (tt_type == EXACT) return tt_score;
                if (tt_type == LOWERBOUND && tt_score >= beta) return tt_score;
                if (tt_type == UPPERBOUND && tt_score <= alpha) return tt_score;
            }
        }

        // Base case: drop into Quiescence Search
        if (depth <= 0) return qsearch(board, geo, alpha, beta, node_count); 

        // --- TASK 6: NULL MOVE PRUNING (NMP) & TASK 8: RFP ---
        int king_idx = board.pieces[board.turn][KING].lsb();
        bool in_check = (king_idx != -1) && MoveGen::is_square_attacked(board, geo, king_idx, (board.turn == WHITE) ? BLACK : WHITE);

        int static_eval = Eval::evaluate(board, geo);
        
        // Reverse Futility Pruning (RFP)
        if (depth <= 4 && !in_check && std::abs(beta) < 28000) {
            int rfp_margin = 120 * depth;
            if (static_eval - rfp_margin >= beta) {
                return static_eval;
            }
        }

        // Null Move Pruning (NMP)
        if (depth >= 3 && !in_check && static_eval >= beta) {
            BoardState nmp_backup = board;
            
            // Pass the turn and flip the hash
            board.turn = (board.turn == WHITE) ? BLACK : WHITE;
            HashEngine::toggle_turn(board.hash_key); 
            
            // Search with a reduced depth (R = 2) and a null window
            int nmp_score = -pvs(board, geo, tt, depth - 1 - 2, -beta, -beta + 1, node_count);
            
            board = nmp_backup; // Restore the state
            
            if (nmp_score >= beta) {
                // If NMP finds a mate score, return beta, not the mate score itself
                return (nmp_score >= 28000) ? beta : nmp_score; 
            }
        }

        // --- STANDARD MOVE GENERATION ---
        MoveList moves;
        MoveGen::generate_legal_moves(board, geo, moves, board.turn);

        if (moves.size() == 0) {
            if (in_check) {
                return -MATE + (100 - depth); // Checkmate!
            }
            return 0; // Stalemate
        }

        // --- TASK 3 & 5: Pass the TT move and Depth into the sorter! ---
        sort_moves(moves, board, tt_move, depth);

        int original_alpha = alpha;
        Move best_move;
        int best_score = -INF;
        bool first_move = true;

        for (size_t i = 0; i < moves.size(); ++i) {
            const auto& m = moves.moves[i];
            BoardState backup = board;
            
            bool is_quiet = (get_victim_piece(board, m.get_to()) == EMPTY) && (m.get_flag() != FLAG_PROMOTION);
            board.execute_move(m, geo, moves.macro_chains[i]);
            
            int score;
            if (m.get_flag() == FLAG_CHAIN_DASH) {
                if (first_move) {
                    score = pvs(board, geo, tt, depth - 1, alpha, beta, node_count);
                } else {
                    score = pvs(board, geo, tt, depth - 1, alpha, alpha + 1, node_count);
                    if (score > alpha && score < beta) {
                        score = pvs(board, geo, tt, depth - 1, alpha, beta, node_count);
                    }
                }
            } else {
                if (first_move) {
                    score = -pvs(board, geo, tt, depth - 1, -beta, -alpha, node_count);
                } else {
                    // --- TASK 7 FIX: SAFER LATE MOVE REDUCTIONS (LMR) ---
                    if (depth >= 3 && i >= 4 && is_quiet && !in_check) {
                        // Less aggressive reduction: only reduce by 2 if extremely deep in the list
                        int reduction = (i > 15) ? 2 : 1;
                        
                        // Prevent the depth from dropping below 1 so it doesn't skip Quiescence blindly!
                        int new_depth = std::max(1, depth - 1 - reduction);
                        
                        // Search with reduced depth and a null window
                        score = -pvs(board, geo, tt, new_depth, -alpha - 1, -alpha, node_count);
                        
                        if (score > alpha) {
                            score = -pvs(board, geo, tt, depth - 1, -alpha - 1, -alpha, node_count);
                        }
                    } else {
                        score = -pvs(board, geo, tt, depth - 1, -alpha - 1, -alpha, node_count);
                    }
                    
                    if (score > alpha && score < beta) {
                        score = -pvs(board, geo, tt, depth - 1, -beta, -alpha, node_count);
                    }
                }
            }
            
            board = backup;
            
            if (score > best_score) {
                best_score = score;
                best_move = m;
            }
            
            if (score > alpha) alpha = score;
            
            // --- TASK 4: STORE THE KILLER MOVE ---
            if (alpha >= beta) {
                if (is_quiet) {
                    killer_moves[depth][1] = killer_moves[depth][0];
                    killer_moves[depth][0] = m;
                }
                break; // Beta cutoff
            }

            first_move = false;
        }
        
        // 2. TRANSPOSITION TABLE STORE
        EntryType type = UPPERBOUND;
        if (best_score >= beta) type = LOWERBOUND;
        else if (best_score > original_alpha) type = EXACT;
        
        tt.store(board.hash_key, depth, best_score, type, best_move);
        
        return best_score;
    }

    // Iterative Deepening with prioritized root move sequences
    inline Move get_best_move(BoardState& board, const BoardGeometry& geo, TranspositionTable& tt, int max_depth) {
        Move absolute_best_move;
        long long total_nodes = 0;
        
        std::cout << "\n==================================================" << std::endl;
        std::cout << "🧠 [AI BRAIN LOG] Initializing Iterative Deepening..." << std::endl;
        std::cout << "==================================================" << std::endl;

        board.print_board(geo);
        Eval::print_eval_breakdown(board, geo);

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int d = 1; d <= max_depth; ++d) {
            std::cout << "\n--- SEARCHING DEPTH " << d << " ---" << std::endl;
            
            MoveList root_moves;
            MoveGen::generate_legal_moves(board, geo, root_moves, board.turn);
            
            // --- TASK 3 FIX: Probe TT to get the best move from the previous depth ---
            int16_t dummy_score;
            EntryType dummy_type;
            Move root_tt_move = Move();
            tt.probe(board.hash_key, 0, dummy_score, dummy_type, root_tt_move);

            // Reorder root moves list prioritizing the TT move
            sort_moves(root_moves, board, root_tt_move, d);
            
            int alpha = -INF;
            int beta = INF;
            Move best_move_this_depth;

            for (size_t i = 0; i < root_moves.size(); ++i) {
                const auto& m = root_moves.moves[i];
                BoardState backup = board;
                
                board.execute_move(m, geo, root_moves.macro_chains[i]);
                
                int score;
                if (m.get_flag() == FLAG_CHAIN_DASH) {
                    score = pvs(board, geo, tt, d - 1, alpha, beta, total_nodes);
                } else {
                    score = -pvs(board, geo, tt, d - 1, -beta, -alpha, total_nodes);
                }
                
                board = backup; 
                
                auto [fx, fy] = geo.index_to_coord(m.get_from());
                auto [tx, ty] = geo.index_to_coord(m.get_to());
                
                if (score > -20000) {
                    std::cout << "   Evaluating: (" << fx << "," << fy << ") -> (" << tx << "," << ty << ") | Score: " << score << std::endl;
                }
                
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
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        double seconds = duration / 1000.0;
        long long nps = seconds > 0 ? static_cast<long long>(total_nodes / seconds) : total_nodes;

        std::cout << "\n================ PERFORMANCE PROFILE ================" << std::endl;
        std::cout << "⏱️  Time taken: " << seconds << " seconds" << std::endl;
        std::cout << "🧮 Total Nodes Evaluated: " << total_nodes << std::endl;
        std::cout << "⚡ Engine Speed (NPS): " << nps << " nodes/sec" << std::endl;
        std::cout << "🚀 Max Depth Reached: " << max_depth << std::endl;
        std::cout << "====================================================\n" << std::endl;

        return absolute_best_move;
    }
}