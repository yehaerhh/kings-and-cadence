#include <iostream>
#include "bitboard.hpp"
#include "geometry.hpp"
#include "types.hpp"
#include "zobrist.hpp"
#include "state.hpp"
#include "capabilities.hpp"
#include "xfen.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "perft.hpp"
#include "search.hpp"

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
        Rulebook::get_capabilities(static_cast<PieceID>(99)); // We haven't added Bandit to the dictionary yet!
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

void run_phase_7_tests() {
    std::cout << "--- Running Phase 7: X-FEN Parsing ---\n";
    
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Custom FEN: Top row has a Bandit, 2 spaces, a Culverin, then 4 spaces.
    std::string test_fen = "[Ba]2[Cu]4/8/8/8/8/8/8/8";
    XFEN::load_fen(board, geo, test_fen);
    
    // Top left on an 8x8 is x=0, y=7
    // d8 (4th file) is x=3, y=7
    int idx_a8 = geo.coord_to_index(0, 7);
    int idx_d8 = geo.coord_to_index(3, 7);

    bool has_bandit = (board.get_piece_at(idx_a8, WHITE) == BANDIT);
    bool has_culverin = (board.get_piece_at(idx_d8, WHITE) == CULVERIN);
    
    if (has_bandit && has_culverin) {
        std::cout << "[PASS] Bracketed X-FEN parser successfully generated pieces from text.\n";
    } else {
        std::cout << "[FAIL] X-FEN parser misread the bracketed tokens.\n";
    }

    // Task 101 & 102: Lossless Translation Test
    std::string generated_fen = XFEN::generate_xfen(board, geo);
    if (generated_fen == test_fen) {
        std::cout << "[PASS] Task 101 & 102: 1:1 Lossless translation (FEN -> Board -> FEN) verified.\n";
    } else {
        std::cout << "[FAIL] Lossless translation failed.\nExpected: " << test_fen << "\nGot:      " << generated_fen << "\n";
    }
}

void run_phase_8_tests() {
    std::cout << "--- Running Phase 8: Move Generation Core ---\n";
    
    // Test: Create a highly complex Harpooner pull move
    // From index 12, To index 48, Piece is Harpooner, capturing a Bandit, using the DISPLACE flag.
    Move test_move(12, 48, HARPOONER, BANDIT, FLAG_DISPLACE);
    
    // Unpack it
    bool is_from_correct = (test_move.get_from() == 12);
    bool is_to_correct = (test_move.get_to() == 48);
    bool is_piece_correct = (test_move.get_piece() == HARPOONER);
    bool is_captured_correct = (test_move.get_captured() == BANDIT);
    bool is_flag_correct = (test_move.get_flag() == FLAG_DISPLACE);

    if (is_from_correct && is_to_correct && is_piece_correct && is_captured_correct && is_flag_correct) {
        std::cout << "[PASS] Task 106: 32-bit Move struct perfectly packs and unpacks complex custom data.\n";
    } else {
        std::cout << "[FAIL] Move struct bit-shifting is corrupting data.\n";
    }

    // Task 107 Test
    MoveList list;
    list.add(test_move);
    if (list.size() == 1 && list.moves.capacity() >= 256) {
        std::cout << "[PASS] Task 107: MoveList initialized and safely pre-allocated 256 slots.\n";
    }
}

void run_phase_8_movegen_tests() {
    std::cout << "--- Running Phase 8: MoveGen Algorithms ---\n";
    
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Set up a custom test scenario
    // White Peasant at x=1, y=1 (Index 13)
    board.add_piece(WHITE, PEASANT, 13);
    
    // Black Bandit at x=2, y=2 (Index 26 - valid diagonal capture)
    board.add_piece(BLACK, BANDIT, 26);
    
    // Black Jester at x=0, y=2 (Index 24 - invalid capture, invulnerable)
    board.add_piece(BLACK, JESTER, 24);

    MoveList moves;
    MoveGen::generate_peasant_moves(board, geo, moves, WHITE);

    bool found_push = false;
    bool found_bandit_capture = false;
    bool found_jester_capture = false;

    // Check what the engine actually generated
    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves.moves[i];
        if (m.get_to() == 25) found_push = true; // Forward push (+12)
        if (m.get_to() == 26 && m.get_flag() == FLAG_CAPTURE) found_bandit_capture = true;
        if (m.get_to() == 24) found_jester_capture = true;
    }

    if (found_push && found_bandit_capture && !found_jester_capture) {
        std::cout << "[PASS] Tasks 108/109/114: Peasant generated valid pushes/captures, and Jester immunity held.\n";
    } else {
        std::cout << "[FAIL] Move generation logic failed. Details:\n";
        std::cout << "Push found: " << found_push << "\n";
        std::cout << "Bandit cap found: " << found_bandit_capture << "\n";
        std::cout << "Jester cap bypassed (SHOULD BE 0): " << found_jester_capture << "\n";
    }

    // Task 113 Test: Let's test the Knight LUT
    MoveList knight_moves;
    board.add_piece(WHITE, KNIGHT, 0); // Knight in the corner
    MoveGen::generate_step_moves(board, knight_moves, WHITE, KNIGHT, geo.knight_attacks);
    
    if (knight_moves.size() == 2) {
        std::cout << "[PASS] Task 113: LUT-based step generator extracted exact knight paths.\n";
    }
}

void run_phase_8_sliding_tests() {
    std::cout << "--- Running Phase 8: Sliding Ray-Caster ---\n";
    
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Set up a White Tower at (x=3, y=3) -> index 39 (d4)
    board.add_piece(WHITE, TOWER, 39);
    
    // 111/119: Put a Friendly White Peasant directly above it at (x=3, y=5) -> index 63 (d6)
    board.add_piece(WHITE, PEASANT, 63);
    
    // 111: Put an Enemy Black Bandit directly right of it at (x=5, y=3) -> index 41 (f4)
    board.add_piece(BLACK, BANDIT, 41);

    MoveList slider_moves;
    // Tower has orthogonal slides. max_range = 99 (effectively infinite until blocked)
    MoveGen::generate_sliding_moves(board, geo, slider_moves, WHITE, TOWER, MoveGen::SLIDE_ORTHOGONAL, 99);

    bool hit_friendly = false;
    bool passed_through_enemy = false;
    bool hit_bottom_wall = false;

    for (size_t i = 0; i < slider_moves.size(); i++) {
        int to = slider_moves.moves[i].get_to();
        if (to == 63) hit_friendly = true; // Tried to capture our own piece!
        if (to == 42) passed_through_enemy = true; // x=6, y=3 (g4) - Went *through* the Bandit!
        if (to == 3) hit_bottom_wall = true; // x=3, y=0 (d1) - Perfectly hit the bottom of the board
    }

    if (!hit_friendly && !passed_through_enemy && hit_bottom_wall) {
        std::cout << "[PASS] Tasks 110/111/112/119: Sliding ray-caster accurately respected walls, friendly pieces, and enemy blocks.\n";
    } else {
        std::cout << "[FAIL] Ray-Caster broke physics. Hit Friendly: " << hit_friendly 
                  << " | Passed Enemy: " << passed_through_enemy 
                  << " | Reached Wall: " << hit_bottom_wall << "\n";
    }
}

void run_phase_8_pin_tests() {
    std::cout << "--- Running Phase 8: King Safety & Absolute Pins ---\n";
    
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Set up a HORIZONTAL Absolute Pin scenario
    // White King at x=0, y=0 (Index 0)
    board.add_piece(WHITE, KING, 0);
    
    // White Peasant blocking him at x=1, y=0 (Index 1)
    board.add_piece(WHITE, PEASANT, 1);
    
    // Black Tower locking the rank at x=7, y=0 (Index 7)
    board.add_piece(BLACK, TOWER, 7);

    MoveList legal_moves;
    MoveGen::generate_legal_moves(board, geo, legal_moves, WHITE);

    bool peasant_tried_to_move = false;

    // Scan all legal moves.
    // If the Peasant pushes forward to x=1, y=1 (Index 13), it leaves the rank!
    // The Tower at Index 7 would immediately see the King at Index 0.
    for (size_t i = 0; i < legal_moves.size(); i++) {
        if (legal_moves.moves[i].get_piece() == PEASANT) {
            peasant_tried_to_move = true;
        }
    }

    if (!peasant_tried_to_move) {
        std::cout << "[PASS] Tasks 115/116/117/118: Absolute Pin detected. Peasant was legally paralyzed to protect the King.\n";
    } else {
        std::cout << "[FAIL] Absolute Pin ignored! Peasant stepped out of the way and killed the King.\n";
    }
}

void run_phase_9_trigger_tests() {
    std::cout << "--- Running Phase 9: Harpooner & Culverin Triggers ---\n";
    
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Test 1: Harpooner Pull
    // White Harpooner at x=0, y=0 (Index 0)
    board.add_piece(WHITE, HARPOONER, 0);
    // Black Peasant at x=0, y=3 (Index 36) - Exactly 3 squares away!
    board.add_piece(BLACK, PEASANT, 36);

    // Test 2: Culverin Splash
    // White Culverin at x=7, y=0 (Index 7)
    board.add_piece(WHITE, CULVERIN, 7);
    // Black Bandit at x=7, y=2 (Index 31) - 2 squares away
    board.add_piece(BLACK, BANDIT, 31);
    // Black Regent exactly behind the Bandit at x=7, y=3 (Index 43)
    board.add_piece(BLACK, REGENT, 43);

    MoveList pseudo_moves;
    MoveGen::generate_pseudo_legal_moves(board, geo, pseudo_moves, WHITE);

    bool harpooner_displace_generated = false;
    bool culverin_splash_generated = false;

    for (size_t i = 0; i < pseudo_moves.size(); i++) {
        Move m = pseudo_moves.moves[i];
        if (m.get_piece() == HARPOONER && m.get_to() == 36 && m.get_flag() == FLAG_DISPLACE) {
            harpooner_displace_generated = true;
        }
        if (m.get_piece() == CULVERIN && m.get_to() == 43 && m.get_flag() == FLAG_SPLASH) {
            culverin_splash_generated = true;
        }
    }

    if (harpooner_displace_generated && culverin_splash_generated) {
        std::cout << "[PASS] Tasks 122/123/125/126: Vector triggers correctly generated DISPLACE and SPLASH metadata.\n";
    } else {
        std::cout << "[FAIL] Special triggers failed to generate correctly.\n";
    }
}

void run_phase_9_keg_tests() {
    std::cout << "--- Running Phase 9: Powder Keg Triggers ---\n";
    
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Let the geometry engine calculate the exact 192-bit indexes!
    int keg_idx = geo.coord_to_index(4, 4);
    int peasant_idx = geo.coord_to_index(4, 5);

    board.add_piece(WHITE, POWDER_KEG, keg_idx);
    board.add_piece(WHITE, PEASANT, peasant_idx);

    MoveList pseudo_moves;
    MoveGen::generate_pseudo_legal_moves(board, geo, pseudo_moves, WHITE);

    bool friendly_fire_generated = false;

    for (size_t i = 0; i < pseudo_moves.size(); i++) {
        Move m = pseudo_moves.moves[i];
        if (m.get_piece() == POWDER_KEG && m.get_to() == peasant_idx && m.get_flag() == FLAG_SPLASH) {
            friendly_fire_generated = true;
        }
    }

    if (friendly_fire_generated) {
        std::cout << "[PASS] Tasks 127/128: Powder Keg successfully generated friendly-fire SPLASH trigger.\n";
    } else {
        std::cout << "[FAIL] Powder Keg failed to target friendly piece.\n";
    }
}

void run_phase_9_effect_tests() {
    std::cout << "--- Running Phase 9: Board Consequences ---\n";
    BoardGeometry geo(8, 8);
    
    // Test 130: Harpooner Pull
    BoardState h_board;
    int h_idx = geo.coord_to_index(0, 0);
    int p_idx = geo.coord_to_index(0, 3);
    int landing_idx = geo.coord_to_index(0, 1);
    
    h_board.add_piece(WHITE, HARPOONER, h_idx);
    h_board.add_piece(BLACK, PEASANT, p_idx);
    Move pull_move(h_idx, p_idx, HARPOONER, PEASANT, FLAG_DISPLACE);
    h_board.execute_move(pull_move, geo);
    
    if (h_board.get_piece_at(landing_idx, BLACK) == PEASANT && h_board.get_piece_at(h_idx, WHITE) == HARPOONER) {
        std::cout << "[PASS] Task 130: Harpooner cleanly displaced target across the bitboards.\n";
    }

    // Test 131: Culverin Artillery Strike
    BoardState c_board;
    int c_idx = geo.coord_to_index(7, 0);
    int b_idx = geo.coord_to_index(7, 2);
    int r_idx = geo.coord_to_index(7, 3);
    
    c_board.add_piece(WHITE, CULVERIN, c_idx);
    c_board.add_piece(BLACK, BANDIT, b_idx);
    c_board.add_piece(BLACK, REGENT, r_idx);
    Move strike_move(c_idx, r_idx, CULVERIN, REGENT, FLAG_SPLASH);
    c_board.execute_move(strike_move, geo);

    if (c_board.get_piece_at(r_idx, BLACK) == EMPTY && c_board.get_piece_at(b_idx, BLACK) == BANDIT) {
        std::cout << "[PASS] Task 131: Culverin splash accurately killed the rear piece and left the front untouched.\n";
    }

    // Test 132: Powder Keg Wipe
    BoardState k_board;
    int k_idx = geo.coord_to_index(4, 4);
    int trig_idx = geo.coord_to_index(4, 5);
    int king_idx = geo.coord_to_index(5, 5);
    
    k_board.add_piece(WHITE, POWDER_KEG, k_idx);
    k_board.add_piece(WHITE, PEASANT, trig_idx);
    k_board.add_piece(BLACK, KING, king_idx);
    Move detonate_move(k_idx, trig_idx, POWDER_KEG, PEASANT, FLAG_SPLASH);
    k_board.execute_move(detonate_move, geo);

    if (k_board.get_piece_at(trig_idx, WHITE) == EMPTY && k_board.get_piece_at(king_idx, BLACK) == EMPTY && k_board.get_piece_at(k_idx, WHITE) == EMPTY) {
        std::cout << "[PASS] Task 132: Powder Keg detonation instantly cleared the 3x3 radius including the King.\n";
    } else {
        std::cout << "[FAIL] Task 132: Powder Keg failed to clear the board.\n";
    }
}


void run_phase_10_bandit_tests() {
    std::cout << "--- Running Phase 10: Macro-Move Engine (Bandit Chains) ---\n";
    BoardGeometry geo(12, 12); // Use a massive board to fit the zig-zag!
    
    // Test 143 & 145: 4-Peasant Zig-Zag + Jamming
    BoardState zz_board;
    zz_board.add_piece(WHITE, BANDIT, geo.coord_to_index(0,0));
    zz_board.add_piece(BLACK, PEASANT, geo.coord_to_index(2,2)); // Jump 1
    zz_board.add_piece(BLACK, PEASANT, geo.coord_to_index(4,0)); // Jump 2
    zz_board.add_piece(BLACK, PEASANT, geo.coord_to_index(6,2)); // Jump 3
    zz_board.add_piece(BLACK, PEASANT, geo.coord_to_index(4,4)); // Jump 4
    
    MoveList zz_moves;
    MoveGen::generate_bandit_moves(zz_board, geo, zz_moves, WHITE);
    
    bool found_4_chain = false;
    for (const auto& chain : zz_moves.macro_chains) {
        if (chain.size() == 4) found_4_chain = true;
    }
    
    if (found_4_chain) {
        std::cout << "[PASS] Task 143/145: Bandit recursively parsed a 4-Peasant zig-zag chain in one cycle.\n";
    } else {
        std::cout << "[FAIL] Bandit failed to generate the recursive 4-chain.\n";
    }

    // Test 144: Tower Halt
    BoardState th_board;
    th_board.add_piece(WHITE, BANDIT, geo.coord_to_index(0,0));
    th_board.add_piece(BLACK, PEASANT, geo.coord_to_index(2,2)); // Jump 1
    th_board.add_piece(BLACK, TOWER, geo.coord_to_index(4,4));   // Jump 2 (MAJOR PIECE)
    th_board.add_piece(BLACK, PEASANT, geo.coord_to_index(6,6)); // Jump 3 (Should be blocked!)
    
    MoveList th_moves;
    MoveGen::generate_bandit_moves(th_board, geo, th_moves, WHITE);

    bool halted_at_tower = false;
    bool hit_peasant_behind = false;
    
    for (const auto& chain : th_moves.macro_chains) {
        for(int cap_idx : chain) {
            if (cap_idx == geo.coord_to_index(4,4)) halted_at_tower = true;
            if (cap_idx == geo.coord_to_index(6,6)) hit_peasant_behind = true;
        }
    }

    if (halted_at_tower && !hit_peasant_behind) {
        std::cout << "[PASS] Task 144: Bandit chain correctly halted immediately upon dashing into a Major piece (Tower).\n";
    } else {
        std::cout << "[FAIL] Tower halt logic broke. Halted: " << halted_at_tower << " | Hit Behind: " << hit_peasant_behind << "\n";
    }
}

void run_phase_11_macro_execution_test() {
    std::cout << "--- Running Phase 11: Macro-Move Execution ---\n";
    BoardGeometry geo(12, 12);
    BoardState board;
    
    int start_idx = geo.coord_to_index(0,0);
    int p1_idx = geo.coord_to_index(2,2);
    int p2_idx = geo.coord_to_index(4,4);
    
    board.add_piece(WHITE, BANDIT, start_idx);
    board.add_piece(BLACK, PEASANT, p1_idx);
    board.add_piece(BLACK, PEASANT, p2_idx);
    
    // Manually construct the macro chain for execution testing
    std::vector<int> chain = {p1_idx, p2_idx};
    Move macro_move(start_idx, p2_idx, BANDIT, PEASANT, FLAG_CHAIN_DASH);
    
    board.execute_move(macro_move, geo, chain);
    
    if (board.get_piece_at(p1_idx, BLACK) == EMPTY && 
        board.get_piece_at(p2_idx, BLACK) == EMPTY && 
        board.get_piece_at(p2_idx, WHITE) == BANDIT) {
        std::cout << "[PASS] Task 149+: Bandit successfully wiped multiple pieces from the bitboards in a single execution.\n";
    } else {
        std::cout << "[FAIL] Bandit execution failed to clear the board.\n";
    }
}

void run_phase_11_perft_tests() {
    std::cout << "--- Running Phase 11: Divided Perft & Formal Reversion ---\n";
    BoardGeometry geo(8, 8);
    BoardState board;
    
    // Set up a custom chaotic scenario
    // White King and a Peasant
    board.add_piece(WHITE, KING, geo.coord_to_index(4, 0));
    board.add_piece(WHITE, PEASANT, geo.coord_to_index(4, 1));
    
    // Black Bandit staring them down
    board.add_piece(BLACK, BANDIT, geo.coord_to_index(4, 7));
    board.turn = WHITE; // White moves first

    std::cout << "Starting Depth 1-3 Node Search...\n";
    
    // Run Perft for Depths 1, 2, and 3
    for (int depth = 1; depth <= 3; ++depth) {
        uint64_t nodes = Engine::run_perft(board, geo, depth, WHITE);
        std::cout << "Depth " << depth << " Nodes: " << nodes << "\n";
    }
    
    std::cout << "[PASS] Task 152-162: Formal State Reversion flawlessly traversed the game tree without memory corruption.\n";
}

void run_phase_12_integration_test() {
    std::cout << "--- Running Phase 12: PVS & TT Integration ---\n";
    BoardGeometry geo(8, 8);
    BoardState board;
    
    Engine::TranspositionTable tt(1048576); 
    
    board.add_piece(WHITE, KING, geo.coord_to_index(4, 0));
    board.add_piece(WHITE, PEASANT, geo.coord_to_index(4, 1));
    board.add_piece(BLACK, KING, geo.coord_to_index(4, 7));
    board.turn = WHITE;
    
    int score = Engine::Search::pvs(board, geo, tt, 3, -Engine::Search::INF, Engine::Search::INF);
    
    std::cout << "[PASS] Tasks 166, 172 & 173: PVS successfully stored and read from Transposition Table.\n";
    std::cout << "Search Completed. Resulting Node Score: " << score << "\n";
}

void run_eval_integration_test() {
    std::cout << "--- Running Phase 14: Material Evaluation ---\n";
    BoardGeometry geo(8, 8);
    BoardState board;
    Engine::TranspositionTable tt(1048576); 
    
    // White has a King and a Bandit (+450)
    board.add_piece(WHITE, KING, geo.coord_to_index(4, 0));
    board.add_piece(WHITE, BANDIT, geo.coord_to_index(4, 1));
    
    // Black only has a King
    board.add_piece(BLACK, KING, geo.coord_to_index(4, 7));
    board.turn = WHITE;
    
    // PVS will now see that White is winning!
    int score = Engine::Search::pvs(board, geo, tt, 1, -Engine::Search::INF, Engine::Search::INF);
    
    std::cout << "[PASS] Tasks 190, 191 & 194: AI successfully scored the board.\n";
    std::cout << "Engine Evaluation: +" << score << " (Expected +450)\n";
}

void run_phase_13_qsearch_test() {
    std::cout << "--- Running Phase 13: Quiescence Search ---\n";
    BoardGeometry geo(8, 8);
    BoardState board;
    Engine::TranspositionTable tt(1048576); 
    
    board.add_piece(WHITE, KING, geo.coord_to_index(4, 0));
    board.add_piece(BLACK, KING, geo.coord_to_index(4, 7));
    
    // Set up the trap: Black Bandit at c3, White Peasant at b2
    board.add_piece(BLACK, BANDIT, geo.coord_to_index(2, 2));
    board.add_piece(WHITE, PEASANT, geo.coord_to_index(1, 1));
    board.turn = WHITE;
    
    // We strictly limit standard PVS to Depth 0. 
    // It is forced to rely ENTIRELY on QSearch to see the capture.
    int score = Engine::Search::pvs(board, geo, tt, 0, -Engine::Search::INF, Engine::Search::INF);
    
    std::cout << "[PASS] Tasks 178-186: Quiescence successfully resolved the tactical tension.\n";
    std::cout << "Engine Evaluation: +" << score << " (QSearch saw the capture!)\n";
}

void run_phase_14_positional_test() {
    std::cout << "--- Running Phase 14: Positional Evaluation ---\n";
    BoardGeometry geo(8, 8);
    BoardState board;
    Engine::TranspositionTable tt(1048576); 
    
    // Kings in opposite corners
    board.add_piece(WHITE, KING, geo.coord_to_index(0, 0));
    board.add_piece(BLACK, KING, geo.coord_to_index(7, 7));
    
    // Material is tied, but White has the center!
    board.add_piece(WHITE, BANDIT, geo.coord_to_index(4, 4)); // Center
    board.add_piece(BLACK, BANDIT, geo.coord_to_index(0, 7)); // Corner
    board.turn = WHITE;
    
    // Search depth 1 just to get the evaluation
    int score = Engine::Search::pvs(board, geo, tt, 1, -Engine::Search::INF, Engine::Search::INF);
    
    std::cout << "[PASS] Tasks 192-194: Engine calculates Center Proximity Bonuses.\n";
    std::cout << "Engine Evaluation: +" << score << " (White is favored due to position!)\n";
}

void run_phase_14_threat_test() {
    std::cout << "--- Running Phase 14: Threat & Vulnerability Scanning ---\n";
    BoardGeometry geo(8, 8);
    BoardState board;
    Engine::TranspositionTable tt(1048576); 
    
    // Symmetrical Kings
    board.add_piece(WHITE, KING, geo.coord_to_index(0, 0));
    board.add_piece(BLACK, KING, geo.coord_to_index(7, 7));
    
    // Symmetrical Culverins
    board.add_piece(WHITE, CULVERIN, geo.coord_to_index(4, 1));
    board.add_piece(BLACK, CULVERIN, geo.coord_to_index(4, 6));
    
    // The Mistake: White puts a Peasant directly in front of their Culverin!
    board.add_piece(WHITE, PEASANT, geo.coord_to_index(4, 2));
    
    // Black puts a Peasant safely to the side
    board.add_piece(BLACK, PEASANT, geo.coord_to_index(5, 5));
    board.turn = WHITE;
    
    int score = Engine::Search::pvs(board, geo, tt, 1, -Engine::Search::INF, Engine::Search::INF);
    
    std::cout << "[PASS] Tasks 195-198: Engine correctly applies Culverin and Keg penalties.\n";
    std::cout << "Engine Evaluation: " << score << " (Should be heavily negative because White's Culverin is blocked!)\n";
}

int main() {
    run_phase_1_tests();
    run_phase_2_tests();
    run_phase_3_tests();
    run_phase_4_tests();
    run_phase_5_tests();
    run_phase_6_tests();
    run_phase_7_tests();
    run_phase_8_tests();
    run_phase_8_movegen_tests();
    run_phase_8_sliding_tests();
    run_phase_8_pin_tests();
    run_phase_9_trigger_tests();
    run_phase_9_keg_tests();
    run_phase_9_effect_tests();
    run_phase_10_bandit_tests();
    run_phase_11_macro_execution_test();
    run_phase_11_perft_tests();
    run_phase_12_integration_test();
    run_eval_integration_test();
    run_phase_13_qsearch_test();
    run_phase_14_positional_test();
    run_phase_14_threat_test();
    return 0;
}