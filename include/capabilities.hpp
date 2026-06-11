#pragma once
#include "types.hpp"
#include <unordered_map>
#include <stdexcept>

namespace Engine {

    // 61. Movement vectors
    enum class MovePattern { STEP, SLIDE, LEAP, NONE };

    // 62. Tactical consequence flags
    enum class CaptureEffect { STANDARD, SPLASH, RESET_TURN, DISPLACE, GHOST, NONE };

    // 63. Trigger condition mapping
    enum class TriggerType { TARGET_RELATIVE, ATTACKER_RELATIVE, NONE };
    
    struct TriggerCondition {
        TriggerType type = TriggerType::NONE;
        int distance = 0; 
    };

    // 64. The Master Capability Struct
    // 75. DOCUMENTATION: This struct maintains strict parity for pybind11 Python exports later.
    struct PieceCapabilities {
        // 65. Primary Moves
        MovePattern primary_pattern = MovePattern::NONE;
        int primary_range = 0;

        // 66. Special Moves (Leaps/Dashes)
        MovePattern special_pattern = MovePattern::NONE;
        int special_min_range = 0;
        int special_max_range = 0;

        // 67. Consequence Logic
        CaptureEffect effect = CaptureEffect::STANDARD;
        
        // 68-71. Behavioral Booleans
        bool disabled_if_adjacent = false; // Bandit jam condition
        bool can_capture_friendly = false; // Powder Keg detonation condition
        bool is_royal = false;             // Regicide (King) condition
        bool is_invulnerable = false;      // Jester wall condition
    };

    namespace Rulebook {
        // 72, 74, 88. Global Static Read-Only Runtime Rulebook mapping all archetypes.
        inline const std::unordered_map<PieceID, PieceCapabilities> piece_registry = {
            { EMPTY, PieceCapabilities() },

            // Baseline Royal (For standard Regicide validation)
            { KING, {
                .primary_pattern = MovePattern::STEP, .primary_range = 1,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = true, .is_invulnerable = false
            }},

            // 76. Map Peasant [Pe]
            { PEASANT, {
                .primary_pattern = MovePattern::STEP, .primary_range = 1,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 77. Map Bandit [Ba]
            { BANDIT, {
                .primary_pattern = MovePattern::STEP, .primary_range = 1,
                .special_pattern = MovePattern::LEAP, .special_min_range = 2, .special_max_range = 3,
                .effect = CaptureEffect::RESET_TURN,
                .disabled_if_adjacent = true, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 78. Map Culverin [Cu]
            { CULVERIN, {
                .primary_pattern = MovePattern::SLIDE, .primary_range = 3,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::SPLASH, // Target-relative +1 distance handled in move-gen
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 79. Map Harpooner [Ha]
            { HARPOONER, {
                .primary_pattern = MovePattern::STEP, .primary_range = 1,
                .special_pattern = MovePattern::SLIDE, .special_min_range = 3, .special_max_range = 3,
                .effect = CaptureEffect::DISPLACE, // Target-relative -2 distance handled in move-gen
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 80. Map Powder Keg [Ke]
            { POWDER_KEG, {
                .primary_pattern = MovePattern::STEP, .primary_range = 1,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::SPLASH, // 8-tile radius handled in move-gen
                .disabled_if_adjacent = false, .can_capture_friendly = true, .is_royal = false, .is_invulnerable = false
            }},

            // 81. Map Tower [To]
            { TOWER, {
                .primary_pattern = MovePattern::SLIDE, .primary_range = 99, // Infinity
                .special_pattern = MovePattern::STEP, .special_min_range = 1, .special_max_range = 1, // Diag buffer
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 82. Map Regent [Re]
            { REGENT, {
                .primary_pattern = MovePattern::STEP, .primary_range = 2,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = true, .is_invulnerable = false
            }},

            // 84. Map Standard Knight [Kn]
            { KNIGHT, {
                .primary_pattern = MovePattern::LEAP, .primary_range = 1,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 85. Map Standard Bishop [Bi]
            { BISHOP, {
                .primary_pattern = MovePattern::SLIDE, .primary_range = 99, // 99 represents Infinite range
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 86. Map Standard Rook [Ro]
            { ROOK, {
                .primary_pattern = MovePattern::SLIDE, .primary_range = 99, 
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 87. Map Standard Queen [Qu]
            { QUEEN, {
                .primary_pattern = MovePattern::SLIDE, .primary_range = 99,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::STANDARD,
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = false
            }},

            // 83. Map Jester [Je]
            { JESTER, {
                .primary_pattern = MovePattern::STEP, .primary_range = 1,
                .special_pattern = MovePattern::NONE, .special_min_range = 0, .special_max_range = 0,
                .effect = CaptureEffect::NONE, // Cannot capture
                .disabled_if_adjacent = false, .can_capture_friendly = false, .is_royal = false, .is_invulnerable = true
            }}
        };

        inline const PieceCapabilities& get_capabilities(PieceID id) {
            try { return piece_registry.at(id); } 
            catch (const std::out_of_range&) { throw std::runtime_error("Capabilities not mapped!"); }
        }
    }
}