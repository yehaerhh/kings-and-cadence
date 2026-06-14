import sys
import os
import argparse
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'build')))
import chess_engine

from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QLabel, QDialog, QPushButton, QHBoxLayout
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtGui import QFont, QPixmap
from board_canvas import BoardCanvas
from engine_worker import EngineWorker

class PieceCache:
    def __init__(self, size=64):
        self.size = size
        self.cache = {}
        # Mapping PieceID (from C++) to file name
        self.mapping = {
            chess_engine.PieceID.PAWN: "pawn",
            chess_engine.PieceID.PEASANT: "peasant", # Ensure these files exist in /assets/pieces/
            chess_engine.PieceID.SOLDIER: "soldier",
            chess_engine.PieceID.KING: "king",
            # ... add all other pieces
        }

    def get_pixmap(self, piece_id, color):
        key = (piece_id, color)
        if key not in self.cache:
            name = self.mapping.get(piece_id, "pawn")
            color_str = "white" if color == chess_engine.Color.WHITE else "black"
            path = f"assets/pieces/{color_str}_{name}.svg"
            
            # Use QSvgRenderer to convert SVG to Pixmap
            renderer = QSvgRenderer(path)
            pixmap = QPixmap(self.size, self.size)
            pixmap.fill(Qt.GlobalColor.transparent)
            painter = QPainter(pixmap)
            renderer.render(painter)
            painter.end()
            self.cache[key] = pixmap
            
        return self.cache[key]

class PromotionDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Promote Pawn")
        self.setModal(True)
        self.setFixedSize(300, 100)
        
        self.selected_piece = None
        
        layout = QHBoxLayout()
        
        # We use the unicode symbols just like the board canvas
        pieces = [
            ("♛ Queen", chess_engine.PieceID.QUEEN),
            ("♜ Rook", chess_engine.PieceID.ROOK),
            ("♝ Bishop", chess_engine.PieceID.BISHOP),
            ("♞ Knight", chess_engine.PieceID.KNIGHT)
        ]
        
        for name, piece_id in pieces:
            btn = QPushButton(name)
            btn.setFont(QFont("sans-serif", 16))
            # The lambda captures the piece_id and triggers our selection method
            btn.clicked.connect(lambda checked, p=piece_id: self.select_piece(p))
            layout.addWidget(btn)
            
        self.setLayout(layout)
        
    def select_piece(self, piece_id):
        self.selected_piece = piece_id
        self.accept()

class MainWindow(QMainWindow):

    def __init__(self, hotseat_mode=False, play_black=False, ai_depth=4):
        super().__init__()
        self.setWindowTitle("Kings and Cadence")
        self.resize(650, 700)
        
        self.hotseat_mode = hotseat_mode
        self.play_black = play_black
        
        # --- THE FIX 1: SAVE THE DEPTH ---
        # We must save the depth to the class so trigger_ai() can access it later!
        self.ai_depth = ai_depth 

        # 1. Spin up Core Engine States
        self.geo = chess_engine.BoardGeometry(8, 8)
        self.board = chess_engine.BoardState()

        # 2. Setup App Interface Layout
        layout = QVBoxLayout()
        
        if self.hotseat_mode:
            initial_text = "Hotseat Mode: Play moves for both sides!"
        else:
            initial_text = "White is thinking..." if self.play_black else "White's Turn - Click a piece to select it."
            
        self.info_panel = QLabel(initial_text)
        self.info_panel.setStyleSheet("font-size: 14px; padding: 10px;")
        self.info_panel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.info_panel.setFixedHeight(50)
        layout.addWidget(self.info_panel)

        # Initialize Canvas (Passing the flip flag!)
        self.canvas = BoardCanvas(self.geo, self.board, flip_board=self.play_black)
        layout.addWidget(self.canvas)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        # 3. Wire Up Interaction Pipelines
        self.canvas.move_played.connect(self.execute_player_move)

        # 4. Populate Tactical Initialization Scenario
        self.setup_initial_scenario()

        # 5. THE AI FIRST-MOVE TRIGGER
        if self.play_black and not self.hotseat_mode:
            self.trigger_ai()

    def trigger_ai(self):
        """Helper function to cleanly wake the AI thread."""
        ai_color_str = "White" if self.board.turn == chess_engine.Color.WHITE else "Black"
        self.info_panel.setText(f"{ai_color_str} is thinking...")
        self.canvas.setEnabled(False) 
        
        board_clone = self.board.clone() 
        
        # --- THE FIX 2: PASS THE SAVED DEPTH TO THE WORKER ---
        # We replace the hardcoded `4` with `self.ai_depth`
        self.worker = EngineWorker(board_clone, self.geo, self.ai_depth) 
        
        self.worker.result_ready.connect(self.execute_ai_move)
        self.worker.start()

    def setup_initial_scenario(self):
        """Inject a full standard 32-piece army with custom pieces replacing center pawns."""
        self.board.turn = chess_engine.Color.WHITE

        # 1. Place the Front Line (Pawns, Peasants, Soldiers)
        for col in range(8):
            b_idx = self.geo.coord_to_index(col, 1) # Black Front Line (Row 1)
            w_idx = self.geo.coord_to_index(col, 6) # White Front Line (Row 6)
            
            if col == 2 or col == 5:
                # c and f files: Peasants (in front of Pontiffs)
                self.board.add_piece(chess_engine.Color.WHITE, chess_engine.PieceID.PEASANT, w_idx)
                self.board.add_piece(chess_engine.Color.BLACK, chess_engine.PieceID.PEASANT, b_idx)
            elif col == 3 or col == 4:
                # d and e files: Soldiers (in front of Queen and King)
                self.board.add_piece(chess_engine.Color.WHITE, chess_engine.PieceID.SOLDIER, w_idx)
                self.board.add_piece(chess_engine.Color.BLACK, chess_engine.PieceID.SOLDIER, b_idx)
            else:
                # a, b, g, h files: Standard Pawns
                self.board.add_piece(chess_engine.Color.WHITE, chess_engine.PieceID.PAWN, w_idx)
                self.board.add_piece(chess_engine.Color.BLACK, chess_engine.PieceID.PAWN, b_idx)

        # 2. Place Standard Back Ranks (with Pontiffs replacing Bishops)
        back_rank = [
            chess_engine.PieceID.ROOK, chess_engine.PieceID.KNIGHT, chess_engine.PieceID.PONTIFF, chess_engine.PieceID.QUEEN,
            chess_engine.PieceID.KING, chess_engine.PieceID.PONTIFF, chess_engine.PieceID.KNIGHT, chess_engine.PieceID.ROOK
        ]

        for col, piece in enumerate(back_rank):
            b_idx = self.geo.coord_to_index(col, 0) # Black Back Rank Top (Row 0)
            w_idx = self.geo.coord_to_index(col, 7) # White Back Rank Bottom (Row 7)
            
            self.board.add_piece(chess_engine.Color.WHITE, piece, w_idx)
            self.board.add_piece(chess_engine.Color.BLACK, piece, b_idx)

        # Grant standard castling rights: 15 = 1 | 2 | 4 | 8 (All 4 options open)
        self.board.castling_rights = 15
        
        # Force UI to read and draw the state we just built
        self.canvas.update()

    def check_game_state(self):
        """Analyzes the board to detect Check, Checkmate, or Stalemate."""
        
        # 1. Ask C++ how many legal moves the CURRENT player has
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        
        # 2. Ask C++ if the CURRENT player's Royals are under attack
        in_check = chess_engine.is_in_check(self.board, self.geo, self.board.turn)
        
        turn_str = "White" if self.board.turn == chess_engine.Color.WHITE else "Black"

        # --- GAME OVER CONDITIONS ---
        if len(legal_moves) == 0:
            if in_check:
                self.info_panel.setText(f"🏆 CHECKMATE! {turn_str} has no escape and loses!")
                self.info_panel.setStyleSheet("font-size: 16px; padding: 10px; font-weight: bold; color: darkred;")
            else:
                self.info_panel.setText(f"🤝 STALEMATE! {turn_str} has no legal moves. It's a draw.")
                self.info_panel.setStyleSheet("font-size: 16px; padding: 10px; font-weight: bold; color: darkblue;")
            
            # Freeze the board forever, the game is over!
            self.canvas.setEnabled(False) 
            return True
            
        # --- ONGOING GAME CONDITIONS ---
        elif in_check:
            self.info_panel.setText(f"⚠️ CHECK! {turn_str}'s Royal is under attack!")
            return False
            
        # Return False meaning the game is still active
        return False

    def execute_player_move(self, from_idx, to_idx):
        # 1. Ask C++ for all mathematically legal moves
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        
        # 2. Find ALL moves that match the player's intended start/end squares
        matching_moves = [m for m in legal_moves if m.get_from() == from_idx and m.get_to() == to_idx]
        
        selected_move = None
        
        # --- PROMOTION INTERCEPTOR ---
        if len(matching_moves) == 1:
            # It's a standard move (only one option)
            selected_move = matching_moves[0]
            
        elif len(matching_moves) > 1:
            # Multiple matches mean it is a promotion!
            from PyQt6.QtWidgets import QDialog 
            
            dialog = PromotionDialog(self)
            if dialog.exec() == QDialog.DialogCode.Accepted:
                chosen_piece = dialog.selected_piece
                for m in matching_moves:
                    if m.get_piece() == chosen_piece:
                        selected_move = m
                        break
            else:
                self.info_panel.setText("Promotion cancelled.")
                return
                
        # 3. Handle Illegal Moves
        if not selected_move:
            self.info_panel.setText("❌ Illegal Move! Try again.")
            return

        # 4. It's Legal! Update the C++ BoardState
        chess_engine.execute_move(self.board, self.geo, selected_move)
        
        # 5. Tell UI to redraw itself immediately to show your move
        self.canvas.update()

        # --- 6. SET DEFAULT TEXT FIRST ---
        if self.hotseat_mode:
            turn_color = "White" if self.board.turn == chess_engine.Color.WHITE else "Black" 
            self.info_panel.setText(f"Hotseat Mode: {turn_color}'s Turn.")
        else:
            self.info_panel.setText("Black is thinking...")

        # --- 7. CHECK GAME STATE ---
        # This will OVERWRITE the default text if there is a Check or Checkmate!
        # If it returns True (Game Over), we exit immediately so the AI doesn't run.
        if self.check_game_state():
            return

        # --- 8. WAKE THE AI (OR PASS TURN IN HOTSEAT) ---
        if self.hotseat_mode:
            turn_color = "White" if self.board.turn == chess_engine.Color.WHITE else "Black" 
            self.info_panel.setText(f"Hotseat Mode: {turn_color}'s Turn.")
        else:
            self.trigger_ai()  # <-- This calls the new function we added earlier!

    def execute_ai_move(self, move_str, time_taken):
        if not move_str or move_str == "ERROR":
            self.info_panel.setText("AI Crashed or found no moves.")
            self.canvas.setEnabled(True)
            return

        # Parse the string returned by the EngineWorker
        clean_str = move_str.replace("(", "").replace(")", "").replace(" ", "")
        parts = clean_str.split("->")
        f_col, f_row = map(int, parts[0].split(","))
        t_col, t_row = map(int, parts[1].split(","))
        
        from_idx = self.geo.coord_to_index(f_col, f_row)
        to_idx = self.geo.coord_to_index(t_col, t_row)
        
        # Find the legal move object for the AI's chosen move
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        selected_move = next((m for m in legal_moves if m.get_from() == from_idx and m.get_to() == to_idx), None)
        
        if selected_move:
            # Execute the AI's move on the REAL board
            chess_engine.execute_move(self.board, self.geo, selected_move)
            self.canvas.update()
            
            # --- 1. CHECK GAME STATE ---
            # If the AI just checkmated YOU, freeze the board and stop here!
            if self.check_game_state():
                return
                
            # --- 2. GIVE TURN BACK TO PLAYER ---
            # Dynamically figure out what color you are playing
            player_color_str = "Black" if self.play_black else "White"
            self.info_panel.setText(f"AI Moved: {move_str}. {player_color_str}'s Turn.")
            self.canvas.setEnabled(True) # Unlock the board so you can play!
        
    def closeEvent(self, event):
        if hasattr(self, 'worker') and self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()
        event.accept()

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Kings and Cadence Chess Engine")
    parser.add_argument('--hotseat', action='store_true', help='Disable AI and play both sides for debugging')
    parser.add_argument('--black', action='store_true', help='Play as Black (AI plays White)')
    parser.add_argument('--depth', type=int, default=4, help='AI Search Depth (default: 4)')
    args = parser.parse_args()

    app = QApplication(sys.argv)
    
    window = MainWindow(hotseat_mode=args.hotseat, play_black=args.black, ai_depth=args.depth)
    window.show()
    sys.exit(app.exec())