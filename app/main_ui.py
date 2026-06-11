import sys
import os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'build')))
import chess_engine

from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QLabel
from PyQt6.QtCore import Qt
from board_canvas import BoardCanvas
from engine_worker import EngineWorker

class MainWindow(QMainWindow):

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Kings and Cadence")
        self.resize(650, 700)

        # 1. Spin up Core Engine States
        self.geo = chess_engine.BoardGeometry(8, 8)
        self.board = chess_engine.BoardState()

        # 2. Setup App Interface Layout
        layout = QVBoxLayout()
        
        self.info_panel = QLabel("White's Turn - Click a piece to select it.")
        self.info_panel.setStyleSheet("font-size: 14px; padding: 10px;")
        self.info_panel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.info_panel.setFixedHeight(50)
        layout.addWidget(self.info_panel)

        # Initialize Canvas
        self.canvas = BoardCanvas(self.geo, self.board)
        layout.addWidget(self.canvas)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        # 3. Wire Up Interaction Pipelines
        self.canvas.move_played.connect(self.execute_player_move)

        # 4. Populate Tactical Initialization Scenario
        self.setup_initial_scenario()

    def setup_initial_scenario(self):
        """Inject a full standard 32-piece army (White on Bottom, Black on Top)."""
        self.board.turn = chess_engine.WHITE

        # 1. Place 16 Peasants (Pawns)
        for col in range(8):
            b_idx = self.geo.coord_to_index(col, 1) # Black Pawns Top
            w_idx = self.geo.coord_to_index(col, 6) # White Pawns Bottom
            
            self.board.add_piece(chess_engine.WHITE, chess_engine.PEASANT, w_idx)
            self.board.add_piece(chess_engine.BLACK, chess_engine.PEASANT, b_idx)
            
            self.canvas.set_piece_mirror(w_idx, 0, chess_engine.PEASANT)
            self.canvas.set_piece_mirror(b_idx, 1, chess_engine.PEASANT)

        # 2. Place Standard Back Ranks
        back_rank = [
            chess_engine.ROOK, chess_engine.KNIGHT, chess_engine.BISHOP, chess_engine.QUEEN,
            chess_engine.KING, chess_engine.BISHOP, chess_engine.KNIGHT, chess_engine.ROOK
        ]

        for col, piece in enumerate(back_rank):
            b_idx = self.geo.coord_to_index(col, 0) # Black Back Rank Top
            w_idx = self.geo.coord_to_index(col, 7) # White Back Rank Bottom
            
            self.board.add_piece(chess_engine.WHITE, piece, w_idx)
            self.board.add_piece(chess_engine.BLACK, piece, b_idx)
            
            self.canvas.set_piece_mirror(w_idx, 0, piece)
            self.canvas.set_piece_mirror(b_idx, 1, piece)

    def execute_player_move(self, from_idx, to_idx):
        f_col, f_row = self.geo.index_to_coord(from_idx)
        t_col, t_row = self.geo.index_to_coord(to_idx)
        
        print(f"\n==================================================")
        print(f"🎯 [PLAYER INTENT] You clicked: ({f_col}, {f_row}) -> ({t_col}, {t_row})")
        print(f"   [RAW INDICES]   from_idx: {from_idx} | to_idx: {to_idx}")

        # 1. Ask C++ for all mathematically legal moves
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        
        # 2. Search the list to see if the player's move exists
        selected_move = None
        for move in legal_moves:
            if move.get_from() == from_idx and move.get_to() == to_idx:
                selected_move = move
                break
                
        # 3. EXTREME REJECTION LOGGING WITH PIECE NAMES
        if not selected_move:
            # Dictionary to translate C++ Piece IDs to English strings
            piece_names = {
                chess_engine.PEASANT: "Peasant",
                chess_engine.KNIGHT: "Knight",
                chess_engine.BISHOP: "Bishop",
                chess_engine.ROOK: "Rook",
                chess_engine.QUEEN: "Queen",
                chess_engine.KING: "King",
                chess_engine.BANDIT: "Bandit"
            }
            
            # Since the engine only generates moves for the current turn, we know the color
            turn_color = "White" if self.board.turn == chess_engine.WHITE else "Black"
            
            print("❌ [ENGINE VERDICT] REJECTED. That move is not in the legal list.")
            print(f"🧠 [ENGINE BRAIN] Here is EXACTLY what {turn_color} can do:")
            
            for i, m in enumerate(legal_moves):
                mf_col, mf_row = self.geo.index_to_coord(m.get_from())
                mt_col, mt_row = self.geo.index_to_coord(m.get_to())
                
                # Fetch the piece ID from the C++ Move object and translate it
                piece_id = m.get_piece()
                piece_name = piece_names.get(piece_id, "Unknown Piece")
                
                print(f"   Move {i+1}: {turn_color} {piece_name} at ({mf_col},{mf_row}) -> ({mt_col},{mt_row})  [Idx: {m.get_from()} -> {m.get_to()}]")
            print("==================================================\n")
            
            self.info_panel.setText("❌ Illegal Move! Check terminal logs.")
            return

        print("✅ [ENGINE VERDICT] ACCEPTED. Updating board state.")
        print("==================================================\n")

        # 4. It's Legal! Update the C++ BoardState
        chess_engine.execute_move(self.board, self.geo, selected_move)
        
        # 5. FORCE THE TURN FLIP TO BLACK
        self.board.turn = chess_engine.BLACK
        
        # 6. Sync the UI Mirror
        if from_idx in self.canvas.pieces:
            color, piece_id = self.canvas.pieces[from_idx]
            self.canvas.set_piece_mirror(from_idx, None, None) 
            self.canvas.set_piece_mirror(to_idx, color, piece_id) 

        # 7. Trigger AI
        self.info_panel.setText("Black is thinking...")
        self.canvas.setEnabled(False) 
        
        self.worker = EngineWorker(self.board, self.geo, 4)
        self.worker.result_ready.connect(self.execute_ai_move)
        self.worker.start()

    def execute_ai_move(self, move_str, time_taken):
        # AI returns an empty string or error if it crashed
        if not move_str or move_str == "ERROR":
            self.info_panel.setText("AI Crashed or found no moves.")
            return

        clean_str = move_str.replace("(", "").replace(")", "").replace(" ", "")
        parts = clean_str.split("->")
        f_col, f_row = map(int, parts[0].split(","))
        t_col, t_row = map(int, parts[1].split(","))
        
        from_idx = self.geo.coord_to_index(f_col, f_row)
        to_idx = self.geo.coord_to_index(t_col, t_row)
        
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        selected_move = next((m for m in legal_moves if m.get_from() == from_idx and m.get_to() == to_idx), None)
        
        if selected_move:
            chess_engine.execute_move(self.board, self.geo, selected_move)
            
            # FORCE TURN BACK TO WHITE
            self.board.turn = chess_engine.WHITE

            if from_idx in self.canvas.pieces:
                color, piece_id = self.canvas.pieces[from_idx]
                self.canvas.set_piece_mirror(from_idx, None, None) 
                self.canvas.set_piece_mirror(to_idx, color, piece_id) 

        self.info_panel.setText(f"AI Moved: {move_str}. White's Turn.")
        self.canvas.setEnabled(True) # Unlock player
        
    def closeEvent(self, event):
        if hasattr(self, 'worker') and self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())