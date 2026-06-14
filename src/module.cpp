#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // Auto-converts C++ std::vector to Python Lists
#include "../include/geometry.hpp"
#include "../include/state.hpp"
#include "../include/search.hpp"
#include "../include/zobrist.hpp"

namespace py = pybind11;

// Helper function to format moves cleanly for Python
std::string move_to_string(const Engine::Move& m, const Engine::BoardGeometry& geo) {
    auto [fx, fy] = geo.index_to_coord(m.get_from());
    auto [tx, ty] = geo.index_to_coord(m.get_to());
    return "(" + std::to_string(fx) + ", " + std::to_string(fy) + ") -> (" + 
                 std::to_string(tx) + ", " + std::to_string(ty) + ")";
}

// 208 & 214. Wrapper for the core AI search with explicit Exception Catching
Engine::Move search_best_move(Engine::BoardState& board, const Engine::BoardGeometry& geo, int depth) {
    try {
        Engine::TranspositionTable tt(4194304); // Spin up the TT for the search
        return Engine::Search::get_best_move(board, geo, tt, depth);
    } catch (const std::exception& e) {
        // 214. Throw cleanly back to Python to prevent hard Segfaults
        throw std::runtime_error(std::string("C++ AI Core crashed: ") + e.what());
    }
}

// 236. Expose the core MoveGen architecture to Python
std::vector<Engine::Move> generate_legal_moves_wrapper(Engine::BoardState& board, const Engine::BoardGeometry& geo) {
    Engine::MoveList moves;
    Engine::MoveGen::generate_legal_moves(board, geo, moves, board.turn);
    
    // We convert our custom MoveList array into a standard std::vector 
    // so pybind11/stl.h can easily pass it to Python as a List!
    std::vector<Engine::Move> python_moves;
    for (size_t i = 0; i < moves.size(); ++i) {
        python_moves.push_back(moves.moves[i]);
    }
    return python_moves;
}

// Helper to execute moves natively from Python
void execute_move_wrapper(Engine::BoardState& board, const Engine::BoardGeometry& geo, const Engine::Move& m) {
    // For now, we pass an empty macro chain. 
    // We will hook up the complex chains later!
    std::vector<int> empty_chain;
    board.execute_move(m, geo, empty_chain);
}

PYBIND11_MODULE(chess_engine, m) {
    m.doc() = "Kings and Cadence Core AI Engine - C++ Backend";

    Engine::HashEngine::init_zobrist();

    // --- THE FIX: Restore Explicit Color Enum Registration ---
    py::enum_<Engine::Color>(m, "Color", py::arithmetic())
        .value("WHITE", Engine::WHITE)
        .value("BLACK", Engine::BLACK)
        .export_values();
    
    // --- EXPLICIT ENUM REGISTRATION (100% Parity with types.hpp) ---
    py::enum_<Engine::PieceID>(m, "PieceID", py::arithmetic())
        .value("KING", Engine::KING)
        .value("QUEEN", Engine::QUEEN)
        .value("ROOK", Engine::ROOK)
        .value("BISHOP", Engine::BISHOP)
        .value("KNIGHT", Engine::KNIGHT)
        .value("PAWN", Engine::PAWN)
        .value("PEASANT", Engine::PEASANT)
        .value("SOLDIER", Engine::SOLDIER)
        .value("BANDIT", Engine::BANDIT)
        .value("CULVERIN", Engine::CULVERIN)
        .value("HARPOONER", Engine::HARPOONER)
        .value("POWDER_KEG", Engine::POWDER_KEG)
        .value("TOWER", Engine::TOWER)
        .value("REGENT", Engine::REGENT)
        .value("JESTER", Engine::JESTER)
        .value("PONTIFF", Engine::PONTIFF)
        .value("EMPTY", Engine::EMPTY)
        .export_values();

    // Expose BoardGeometry
    py::class_<Engine::BoardGeometry>(m, "BoardGeometry")
        .def(py::init<int, int>(), py::arg("width"), py::arg("height"))
        .def_readwrite("active_width", &Engine::BoardGeometry::active_width)
        .def_readwrite("active_height", &Engine::BoardGeometry::active_height)
        .def("coord_to_index", &Engine::BoardGeometry::coord_to_index)
        .def("index_to_coord", [](const Engine::BoardGeometry& geo, int index) {
            auto [x, y] = geo.index_to_coord(index);
            return py::make_tuple(x, y);
        });

    // Expose the Move object
    py::class_<Engine::Move>(m, "Move")
        .def(py::init<>())
        .def("get_from", &Engine::Move::get_from)
        .def("get_to", &Engine::Move::get_to)
        .def("get_piece", &Engine::Move::get_piece)
        .def("get_flag", &Engine::Move::get_flag)
        .def("to_string", &move_to_string);

    // Expose BoardState 
    py::class_<Engine::BoardState>(m, "BoardState")
        .def(py::init<>())
        .def_readwrite("turn", &Engine::BoardState::turn)
        .def_readwrite("castling_rights", &Engine::BoardState::castling_rights)
        .def("add_piece", &Engine::BoardState::add_piece)
        .def("get_piece_at", &Engine::BoardState::get_piece_at)
        .def("clone", [](const Engine::BoardState& b){return Engine::BoardState(b);}, "Creates a deep copy of the board state");
    // Bind the Move Generator & Execution
    m.def("generate_legal_moves", &generate_legal_moves_wrapper, "Get all legal moves");
    m.def("is_in_check", &Engine::MoveGen::is_in_check, "Check if the specified color's Royal pieces are under attack");
    m.def("execute_move", &execute_move_wrapper, "Apply a move to the C++ BoardState");

    // --- THE FIX: RELEASE THE GIL ---
    // This allows Python to keep the UI responsive while C++ maxes out the CPU
    m.def("get_best_move", &search_best_move, "Calculate the best move using PVS",
          py::arg("board"), py::arg("geo"), py::arg("depth"),
          py::call_guard<py::gil_scoped_release>()); 
}