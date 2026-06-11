#pragma once
#include "state.hpp"
#include "movegen.hpp"
#include <iostream>

namespace Engine {

    // The Recursive Performance Tester
    inline uint64_t run_perft(BoardState& board, const BoardGeometry& geo, int depth, Color us) {
        if (depth == 0) return 1ULL; // We reached the bottom of this branch, count it as 1 node.

        MoveList moves;
        MoveGen::generate_legal_moves(board, geo, moves, us);

        uint64_t nodes = 0;

        for (size_t i = 0; i < moves.size(); i++) {
            // 152 & 153. Formal State Snapshot (Backup the 192-bit bitboards)
            BoardState backup = board; 
            
            // Execute the physical move (including any explosive macro chains)
            board.execute_move(moves.moves[i], geo, moves.macro_chains[i]);
            
            // Recurse to the next depth. Notice we pass `board.turn`, because 
            // execute_move is smart enough to know if a Bandit Chain Dash kept the turn!
            nodes += run_perft(board, geo, depth - 1, board.turn);
            
            // 154. Formal State Reversion (Restore the bitboards instantly)
            board = backup; 
        }

        return nodes;
    }
}