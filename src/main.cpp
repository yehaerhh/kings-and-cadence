#include <iostream>
#include "bitboard.hpp"

using namespace Engine;

void run_phase_1_tests() {
    std::cout << "--- Running Phase 1: Bitboard Diagnostics ---\n";
    
    Bitboard192 bb;

    // Test 1: Set edge case index 143 (Top-right corner of a 12x12 board)
    bb.set_bit(143);
    
    // 143 / 64 = 2 (data[2])
    // 143 % 64 = 15 (15th bit)
    bool is_in_data2 = (bb.data[2] != 0);
    bool is_15th_bit = (bb.data[2] == (1ULL << 15));

    if (is_in_data2 && is_15th_bit) {
        std::cout << "[PASS] Index 143 mapped correctly to data[2], bit 15.\n";
    } else {
        std::cout << "[FAIL] Index 143 mapping is completely broken.\n";
    }

    // Test 2: Test Popcount and LSB
    bb.set_bit(0);  // 1st bit of data[0]
    bb.set_bit(65); // 2nd bit of data[1]

    if (bb.popcount() == 3) {
        std::cout << "[PASS] Hardware popcount registered 3 pieces.\n";
    } else {
        std::cout << "[FAIL] Popcount registered: " << bb.popcount() << "\n";
    }

    if (bb.lsb() == 0) {
        std::cout << "[PASS] Hardware LSB scanner found index 0 first.\n";
    } else {
        std::cout << "[FAIL] LSB found index: " << bb.lsb() << "\n";
    }
}

int main() {
    run_phase_1_tests();
    return 0;
}