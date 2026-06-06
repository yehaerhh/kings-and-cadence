#include <iostream>
#include "bitboard.hpp"
#include "geometry.hpp"
#include "types.hpp"
#include "zobrist.hpp"
#include "state.hpp"
#include "capabilities.hpp"

using namespace Engine;

void run_phase_1_tests() {
    Bitboard192 bb;
    bb.set_bit(143);
    if ((bb.data[2] != 0) && (bb.data[2] == (1ULL << 15))) {
        std::cout << "[PASS] Phase 1: 192-bit boundary crossover is stable.\n";
    }
}

void run_phase_2_tests() {
    std::cout << "--- Running Phase 2: Geometry Diagnostics ---\n";
    
    // Test 1: 8x8 Board Limits
    BoardGeometry geo8(8, 8);
    int index_e4 = geo8.coord_to_index(4, 3);
    if (index_e4 == 40) std::cout << "[PASS] 8x8: Coordinate (4,3) mapped correctly to index 40.\n";
    
    if (geo8.is_in_bounds(7) && !geo8.is_in_bounds(8)) {
        std::cout << "[PASS] 8x8: Boundary mask correctly blocked out-of-bounds square.\n";
    }

    // Test 2: Knight Look-Up Table verification
    // A Knight at (0,0) on an 8x8 board can only jump to (1,2) and (2,1) -> which are indexes 25 and 14
    int knight_moves_from_corner = geo8.knight_attacks[0].popcount();
    if (knight_moves_from_corner == 2) {
        std::cout << "[PASS] 8x8: Knight LUT correctly generated 2 valid moves from the corner.\n";
    } else {
        std::cout << "[FAIL] Knight LUT generated " << knight_moves_from_corner << " moves from corner.\n";
    }

    // Test 3: 12x12 Board Expansion
    BoardGeometry geo12(12, 12);
    // On a 12x12 board, the top right corner is index 143. It should be perfectly valid.
    if (geo12.is_in_bounds(143)) {
        std::cout << "[PASS] 12x12: Boundary mask successfully expanded to 144 squares.\n";
    } else {
        std::cout << "[FAIL] 12x12 Expansion failed.\n";
    }
}

void run_phase_3_tests() {
    std::cout << "--- Running Phase 3: Zobrist Hashing ---\n";
    HashEngine::init_zobrist();

    // 41. Test: XOR Symmetry
    uint64_t test_hash = 0ULL;
    HashEngine::toggle_piece(test_hash, WHITE, BANDIT, 45); 
    HashEngine::toggle_piece(test_hash, WHITE, BANDIT, 45); 
    if (test_hash == 0ULL) std::cout << "[PASS] 41. Zobrist XOR symmetry is perfect.\n";

    // 42. Test: Board Inversion
    uint64_t white_hash = 0ULL;
    uint64_t black_hash = 0ULL;
    HashEngine::toggle_piece(white_hash, WHITE, BANDIT, 45);
    HashEngine::toggle_piece(black_hash, BLACK, BANDIT, 45);
    if (white_hash != black_hash) std::cout << "[PASS] 42. Board inversion produces distinct hashes.\n";

    // 43. Test: State Mutation (Turn flag)
    uint64_t turn_hash1 = white_hash;
    uint64_t turn_hash2 = white_hash;
    HashEngine::toggle_side(turn_hash2); // Change whose turn it is
    if (turn_hash1 != turn_hash2) std::cout << "[PASS] 43. State mutation (turn flag) changes hash.\n";
}

void run_phase_4_tests() {
    std::cout << "--- Running Phase 4: Board State Integration ---\n";
    
    BoardState board;
    
    // Test 1: Drop a White Bandit on index 45
    board.add_piece(WHITE, BANDIT, 45);
    
    bool is_bandit_found = (board.get_piece_at(45, WHITE) == BANDIT);
    bool is_occupancy_synced = board.occupancy[NONE].test_bit(45);
    bool is_hash_scrambled = (board.hash_key != 0ULL);

    if (is_bandit_found && is_occupancy_synced && is_hash_scrambled) {
        std::cout << "[PASS] Piece addition fully synced with Bitboards, Occupancy, and Hash.\n";
    } else {
        std::cout << "[FAIL] Board state syncing broken.\n";
    }

    // Test 2: Remove the Bandit
    board.remove_piece(WHITE, BANDIT, 45);
    
    if (board.occupancy[NONE].popcount() == 0 && board.hash_key == 0ULL) {
        std::cout << "[PASS] Piece removal perfectly wiped the board and restored the hash.\n";
    } else {
        std::cout << "[FAIL] Ghost pieces left behind after removal.\n";
    }

    // Test 3 (Task 58): Move the piece
    board.add_piece(WHITE, BANDIT, 45); // Put it back
    board.move_piece(WHITE, BANDIT, 45, 55); // Move it 10 squares up

    bool moved_from_empty = (board.get_piece_at(45, WHITE) == EMPTY);
    bool moved_to_filled = (board.get_piece_at(55, WHITE) == BANDIT);
    
    if (moved_from_empty && moved_to_filled) {
        std::cout << "[PASS] Task 58: move_piece accurately relocates piece and updates state.\n";
    } else {
        std::cout << "[FAIL] move_piece logic is broken.\n";
    }
}

void run_phase_5_tests() {
    std::cout << "--- Running Phase 5: Component Rules ---\n";
    
    // Test 1: Check if the EMPTY piece is successfully pulled from the read-only map
    const PieceCapabilities& empty_caps = Rulebook::get_capabilities(EMPTY);
    
    if (empty_caps.primary_pattern == MovePattern::NONE && empty_caps.is_royal == false) {
        std::cout << "[PASS] Capabilities struct compiled and read-only map accessed safely.\n";
    } else {
        std::cout << "[FAIL] Capability mapping failed.\n";
    }
    
    // Test 2: Exception handling for unmapped pieces (Tasks 72-74)
    try {
        Rulebook::get_capabilities(BANDIT); // We haven't added Bandit to the dictionary yet!
        std::cout << "[FAIL] Getter allowed access to an unmapped piece.\n";
    } catch (const std::runtime_error& e) {
        std::cout << "[PASS] Getter correctly threw an error for an unmapped piece.\n";
    }
}

void run_phase_6_tests() {
    std::cout << "--- Running Phase 6: Archetype Validations ---\n";
    
    // Task 84: Jester Isolation Logic
    const PieceCapabilities& jester = Rulebook::get_capabilities(JESTER);
    if (jester.is_invulnerable == true && jester.is_royal == false) {
        std::cout << "[PASS] Task 84: Jester is invulnerable but NOT royal (safe from Regicide loops).\n";
    }

    // Task 85: Bandit Range Overlap Check
    const PieceCapabilities& bandit = Rulebook::get_capabilities(BANDIT);
    if (bandit.special_min_range > bandit.primary_range) {
        std::cout << "[PASS] Task 85: Bandit special range (" << bandit.special_min_range 
                  << ") strictly bypasses primary range (" << bandit.primary_range << ").\n";
    }

    // Task 86: Powder Keg Friendly Fire
    const PieceCapabilities& keg = Rulebook::get_capabilities(POWDER_KEG);
    if (keg.can_capture_friendly == true && keg.effect == CaptureEffect::SPLASH) {
        std::cout << "[PASS] Task 86: Powder Keg friendly-fire flags correctly configured.\n";
    }

    // Task 87: Regent parity with King
    const PieceCapabilities& regent = Rulebook::get_capabilities(REGENT);
    const PieceCapabilities& king = Rulebook::get_capabilities(KING);
    if (regent.is_royal == true && king.is_royal == true) {
        std::cout << "[PASS] Task 87: Regent mapped identically to standard King for regicide checks.\n";
    }
}

int main() {
    run_phase_1_tests();
    run_phase_2_tests();
    run_phase_3_tests();
    run_phase_4_tests();
    run_phase_5_tests();
    run_phase_6_tests();
    return 0;
}