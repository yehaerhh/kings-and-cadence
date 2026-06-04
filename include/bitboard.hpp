#pragma once
#include <cstdint>
#include <bit>      // For C++20 hardware bit-scanning
#include <cassert>

namespace Engine {

    // Absolute Maximum Geometry Limits
    constexpr int MAX_WIDTH = 12;
    constexpr int MAX_HEIGHT = 12;
    constexpr int MAX_SQUARES = MAX_WIDTH * MAX_HEIGHT; // 144 Squares

    // Directional Offsets (Assuming 1D array mapping)
    enum Direction : int {
        NORTH = MAX_WIDTH, SOUTH = -MAX_WIDTH,
        EAST  = 1,         WEST  = -1,
        NE    = MAX_WIDTH + 1, NW = MAX_WIDTH - 1,
        SE    = -MAX_WIDTH + 1, SW = -MAX_WIDTH - 1
    };

    // 192-Bit Matrix Structure
    struct alignas(32) Bitboard192 {
        uint64_t data[3] = {0ULL, 0ULL, 0ULL};

        // --- Bitwise Operators ---
        
        inline Bitboard192 operator~() const {
            return {~data[0], ~data[1], ~data[2]};
        }

        inline Bitboard192 operator&(const Bitboard192& other) const {
            return {data[0] & other.data[0], data[1] & other.data[1], data[2] & other.data[2]};
        }

        inline Bitboard192 operator|(const Bitboard192& other) const {
            return {data[0] | other.data[0], data[1] | other.data[1], data[2] | other.data[2]};
        }

        inline Bitboard192 operator^(const Bitboard192& other) const {
            return {data[0] ^ other.data[0], data[1] ^ other.data[1], data[2] ^ other.data[2]};
        }

        inline bool operator==(const Bitboard192& other) const {
            return data[0] == other.data[0] && data[1] == other.data[1] && data[2] == other.data[2];
        }

        // --- Core Primitives ---

        inline void set_bit(int index) {
            assert(index >= 0 && index < MAX_SQUARES);
            data[index / 64] |= (1ULL << (index % 64));
        }

        inline void clear_bit(int index) {
            assert(index >= 0 && index < MAX_SQUARES);
            data[index / 64] &= ~(1ULL << (index % 64));
        }

        inline bool test_bit(int index) const {
            assert(index >= 0 && index < MAX_SQUARES);
            return (data[index / 64] & (1ULL << (index % 64))) != 0;
        }

        // --- Hardware Accelerated Scanners ---

        // Counts total number of 1s (Used for material evaluation)
        inline int popcount() const {
            return std::popcount(data[0]) + std::popcount(data[1]) + std::popcount(data[2]);
        }

        // Finds the Least Significant Bit (Used for iterating through pieces)
        inline int lsb() const {
            if (data[0]) return std::countr_zero(data[0]);
            if (data[1]) return 64 + std::countr_zero(data[1]);
            if (data[2]) return 128 + std::countr_zero(data[2]);
            return -1; // Empty bitboard
        }
    };

} // namespace Engine