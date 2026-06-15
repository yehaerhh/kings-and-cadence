import sys
import os
import argparse

# Ensure C++ build paths are set correctly
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'build')))
import chess_engine

from PyQt6.QtWidgets import (QApplication, QMainWindow, QVBoxLayout, QWidget, 
                             QLabel, QDialog, QPushButton, QHBoxLayout, 
                             QStackedWidget, QSlider, QFrame, QComboBox)
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtGui import QFont, QPixmap, QPainter
from PyQt6.QtSvg import QSvgRenderer

from board_canvas import BoardCanvas
from engine_worker import EngineWorker
from graveyard import GraveyardWidget
# And make sure PieceCache is either in main_ui.py or imported if you moved it!


class PieceCache:
    def __init__(self, size=64):
        self.size = size
        self.cache = {}
        self.mapping = {
            chess_engine.PieceID.PAWN: "pawn",
            chess_engine.PieceID.PEASANT: "peasant", 
            chess_engine.PieceID.SOLDIER: "soldier",
            chess_engine.PieceID.KING: "king",
        }

    def get_pixmap(self, piece_id, color):
        key = (piece_id, color)
        if key not in self.cache:
            name = self.mapping.get(piece_id, "pawn")
            color_str = "white" if color == chess_engine.Color.WHITE else "black"
            path = f"assets/pieces/{color_str}_{name}.svg"
            
            renderer = QSvgRenderer(path)
            pixmap = QPixmap(self.size, self.size)
            pixmap.fill(Qt.GlobalColor.transparent)
            painter = QPainter(pixmap)
            renderer.render(painter)
            painter.end()
            self.cache[key] = pixmap
            
        return self.cache[key]


class GameSetupDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Game Setup")
        self.setModal(True)
        self.setFixedSize(300, 150)
        self.setStyleSheet("background-color: #2C3E50; color: white;")
        
        layout = QVBoxLayout()
        
        label = QLabel("Select Board Size:")
        label.setFont(QFont("Arial", 14, QFont.Weight.Bold))
        layout.addWidget(label)
        
        self.size_combo = QComboBox()
        self.size_combo.addItems(["8x8 Classic", "10x10 Vanguard", "12x12 Grand"])
        self.size_combo.setStyleSheet("""
            QComboBox { background-color: #1A1A1A; color: white; font-size: 14px; padding: 5px; border-radius: 4px; }
            QComboBox QAbstractItemView { background-color: #1A1A1A; color: white; selection-background-color: #1ABC9C; }
        """)
        layout.addWidget(self.size_combo)
        
        btn = QPushButton("⚔️ Start Game")
        btn.setStyleSheet("""
            QPushButton { background-color: #27AE60; font-size: 16px; font-weight: bold; border-radius: 4px; padding: 10px; }
            QPushButton:hover { background-color: #2ECC71; }
        """)
        btn.clicked.connect(self.accept)
        layout.addWidget(btn)
        
        self.setLayout(layout)

class PromotionDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Promote Pawn")
        self.setModal(True)
        self.setFixedSize(300, 100)
        
        self.selected_piece = None
        layout = QHBoxLayout()
        
        pieces = [
            ("♛ Queen", chess_engine.PieceID.QUEEN),
            ("♜ Rook", chess_engine.PieceID.ROOK),
            ("♝ Bishop", chess_engine.PieceID.BISHOP),
            ("♞ Knight", chess_engine.PieceID.KNIGHT)
        ]
        
        for name, piece_id in pieces:
            btn = QPushButton(name)
            btn.setFont(QFont("sans-serif", 16))
            btn.clicked.connect(lambda checked, p=piece_id: self.select_piece(p))
            layout.addWidget(btn)
            
        self.setLayout(layout)
        
    def select_piece(self, piece_id):
        self.selected_piece = piece_id
        self.accept()


class HomeMenu(QWidget):
    def __init__(self, start_game_callback, open_settings_callback):
        super().__init__()
        self.start_game_callback = start_game_callback # Save this for later!
        layout = QVBoxLayout()
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        title = QLabel("KINGS & CADENCE")
        title.setFont(QFont("Arial", 38, QFont.Weight.Bold))
        title.setStyleSheet("color: #E0E0E0; letter-spacing: 2px; margin-bottom: 30px;")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)

        button_style = """
            QPushButton {
                background-color: #2C3E50; color: #FFFFFF; font-size: 16px;
                font-weight: bold; border-radius: 6px; padding: 14px;
                min-width: 280px; margin: 8px;
            }
            QPushButton:hover { background-color: #34495E; }
            QPushButton:pressed { background-color: #1ABC9C; }
        """

        btn_computer = QPushButton("🤖 Play vs Computer")
        btn_computer.setStyleSheet(button_style)
        btn_computer.clicked.connect(lambda: self.trigger_game_setup(hotseat=False, play_black=False))
        layout.addWidget(btn_computer)

        btn_friend = QPushButton("🤝 Challenge a Friend (Local)")
        btn_friend.setStyleSheet(button_style)
        btn_friend.clicked.connect(lambda: self.trigger_game_setup(hotseat=True, play_black=False))
        layout.addWidget(btn_friend)

        btn_settings = QPushButton("⚙️ Engine Settings")
        btn_settings.setStyleSheet(button_style)
        btn_settings.clicked.connect(open_settings_callback)
        layout.addWidget(btn_settings)

        self.setLayout(layout)

    def trigger_game_setup(self, hotseat, play_black):
        """Pops up the board size dialog before launching the game."""
        dialog = GameSetupDialog(self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            # Extract the integer from the dropdown text (e.g., "10x10 Vanguard" -> 10)
            size_str = dialog.size_combo.currentText()
            board_size = int(size_str.split("x")[0])
            self.start_game_callback(hotseat, play_black, board_size)


# --- NEW COMPONENT: SETTINGS MENU ---
class SettingsMenu(QWidget):
    def __init__(self, back_callback):
        super().__init__()
        self.back_callback = back_callback
        layout = QVBoxLayout()
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        title = QLabel("ENGINE CONFIGURATION")
        title.setFont(QFont("Arial", 22, QFont.Weight.Bold))
        title.setStyleSheet("color: #E0E0E0; margin-bottom: 30px;")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)

        # AI Depth Setting Slider Control
        self.depth_label = QLabel("AI Search Depth: 4")
        self.depth_label.setStyleSheet("color: #FFFFFF; font-size: 14px; margin-top: 10px;")
        self.depth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.depth_label)

        self.depth_slider = QSlider(Qt.Orientation.Horizontal)
        self.depth_slider.setMinimum(1)
        self.depth_slider.setMaximum(6)
        self.depth_slider.setValue(4)
        self.depth_slider.setTickPosition(QSlider.TickPosition.TicksBelow)
        self.depth_slider.setTickInterval(1)
        self.depth_slider.setFixedWidth(260)
        self.depth_slider.valueChanged.connect(self.update_depth_text)
        layout.addWidget(self.depth_slider)

        # Save & Back Button Setup
        btn_back = QPushButton("💾 Save & Return")
        btn_back.setStyleSheet("""
            QPushButton {
                background-color: #27AE60;
                color: white;
                font-size: 14px;
                font-weight: bold;
                border-radius: 4px;
                padding: 10px;
                margin-top: 40px;
                min-width: 150px;
            }
            QPushButton:hover { background-color: #2ECC71; }
        """)
        btn_back.clicked.connect(self.back_callback)
        layout.addWidget(btn_back)

        self.setLayout(layout)

    def update_depth_text(self, value):
        self.depth_label.setText(f"AI Search Depth: {value}")

    def get_depth(self):
        return self.depth_slider.value()


class MainWindow(QMainWindow):
    def __init__(self, hotseat_mode=False, play_black=False, ai_depth=4, bypass_menu=False):
        super().__init__()
        self.setWindowTitle("Kings and Cadence")
        self.resize(650, 720)
        self.setStyleSheet("background-color: #1A1A1A;") 

        # Unified Application Window Engine Variables
        self.hotseat_mode = hotseat_mode
        self.play_black = play_black
        self.ai_depth = ai_depth 

        # High-level Stack View Structure Layout Integration
        self.stack = QStackedWidget()
        self.setCentralWidget(self.stack)

        # Instantiating Sub-Menu Views
        self.home_screen = HomeMenu(self.launch_game_session, self.show_settings_view)
        self.settings_screen = SettingsMenu(self.show_home_view)

        self.stack.addWidget(self.home_screen)      # Index 0
        self.stack.addWidget(self.settings_screen)  # Index 1

        # Check if direct CLI parameters should bypass Home UI
        if bypass_menu:
            self.launch_game_session(self.hotseat_mode, self.play_black)
        else:
            self.show_home_view()

    def show_home_view(self):
        self.stack.setCurrentIndex(0)

    def show_settings_view(self):
        self.stack.setCurrentIndex(1)

    def launch_game_session(self, hotseat, play_black, board_size=8):
        """Prepares canvas, loads engine states, and switches visibility into active game frame."""
        self.hotseat_mode = hotseat
        self.play_black = play_black
        
        # Instantiate the Cache once per app lifecycle so it doesn't reload SVGs constantly
        if not hasattr(self, 'piece_cache'):
            self.piece_cache = PieceCache()
        
        # Read the current customized AI Depth out of the Settings view layer
        if self.stack.indexOf(self.settings_screen) != -1:
            self.ai_depth = self.settings_screen.get_depth()

        # Spin up Engine Core Variables
        self.geo = chess_engine.BoardGeometry(board_size, board_size)
        self.board = chess_engine.BoardState()

        # Build Game Runtime Main Interface Layout
        game_container = QWidget()
        game_layout = QVBoxLayout()
        
        if self.hotseat_mode:
            initial_text = "Hotseat Mode: Play moves for both sides!"
        else:
            initial_text = "White is thinking..." if self.play_black else "White's Turn - Click a piece to select it."
            
        self.info_panel = QLabel(initial_text)
        self.info_panel.setStyleSheet("font-size: 14px; padding: 10px; color: #FFFFFF;")
        self.info_panel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.info_panel.setFixedHeight(50)
        game_layout.addWidget(self.info_panel)

        # --- NEW: ADD GRAVEYARDS ---
        # Top Graveyard (Shows White pieces captured by Black)
        self.top_graveyard = GraveyardWidget(self.piece_cache, chess_engine.Color.WHITE)
        # Bottom Graveyard (Shows Black pieces captured by White)
        self.bottom_graveyard = GraveyardWidget(self.piece_cache, chess_engine.Color.BLACK)
        
        game_layout.addWidget(self.top_graveyard)

        # Initialize Canvas Component (NOW PASSING THE CACHE)
        self.canvas = BoardCanvas(self.geo, self.board, flip_board=self.play_black)
        game_layout.addWidget(self.canvas)
        
        game_layout.addWidget(self.bottom_graveyard)

        # Interface Navigation Panel bar (Exit/Reset Option)
        control_bar = QHBoxLayout()
        btn_exit = QPushButton("🏳️ Exit Game")
        btn_exit.setStyleSheet("""
            QPushButton { color: #E74C3C; font-weight: bold; padding: 6px 12px; background: transparent; border: 1px solid #E74C3C; border-radius: 4px; }
            QPushButton:hover { background: #E74C3C; color: white; }
        """)
        btn_exit.clicked.connect(self.terminate_and_return_to_menu)
        control_bar.addWidget(btn_exit)
        control_bar.addStretch()
        game_layout.addLayout(control_bar)

        game_container.setLayout(game_layout)

        # Append Game Frame View directly onto Stack Interface context indexes
        game_idx = self.stack.addWidget(game_container)
        self.stack.setCurrentIndex(game_idx)

        # Wire Up Canvas Interaction Connections Pipeline
        self.canvas.move_played.connect(self.execute_player_move)

        # Populate Engine Formations Map Space Matrix
        self.setup_initial_scenario()

        # Initialize Active Worker thread state if player choice requires AI opening move
        if self.play_black and not self.hotseat_mode:
            self.trigger_ai()

    def terminate_and_return_to_menu(self):
        """Kills background processing workers gracefully before removing active canvas objects from frame stack."""
        if hasattr(self, 'worker') and self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()
            
        current_widget = self.stack.currentWidget()
        self.show_home_view()
        self.stack.removeWidget(current_widget)
        current_widget.deleteLater()

    def trigger_ai(self):
        ai_color_str = "White" if self.board.turn == chess_engine.Color.WHITE else "Black"
        self.info_panel.setText(f"{ai_color_str} is thinking...")
        self.canvas.setEnabled(False) 
        
        board_clone = self.board.clone() 
        self.worker = EngineWorker(board_clone, self.geo, self.ai_depth) 
        
        self.worker.result_ready.connect(self.execute_ai_move)
        self.worker.start()

    def setup_initial_scenario(self):
        self.board.turn = chess_engine.Color.WHITE
        size = self.geo.active_width # Get dynamic board size

        # 1. Dynamic Front Line Setup
        for col in range(size):
            b_idx = self.geo.coord_to_index(col, 1) 
            w_idx = self.geo.coord_to_index(col, size - 2) # e.g. Row 6 for 8x8, Row 8 for 10x10
            
            # Keep special pieces centered relative to the board size
            mid = size // 2
            if col == mid - 2 or col == mid + 1:
                self.board.add_piece(chess_engine.Color.WHITE, chess_engine.PieceID.PEASANT, w_idx)
                self.board.add_piece(chess_engine.Color.BLACK, chess_engine.PieceID.PEASANT, b_idx)
            elif col == mid - 1 or col == mid:
                self.board.add_piece(chess_engine.Color.WHITE, chess_engine.PieceID.SOLDIER, w_idx)
                self.board.add_piece(chess_engine.Color.BLACK, chess_engine.PieceID.SOLDIER, b_idx)
            else:
                self.board.add_piece(chess_engine.Color.WHITE, chess_engine.PieceID.PAWN, w_idx)
                self.board.add_piece(chess_engine.Color.BLACK, chess_engine.PieceID.PAWN, b_idx)

        # 2. Dynamic Back Rank Setup
        # ---> THIS IS THE LIST THAT WENT MISSING! <---
        base_back_rank = [
            chess_engine.PieceID.ROOK, chess_engine.PieceID.KNIGHT, chess_engine.PieceID.PONTIFF, chess_engine.PieceID.QUEEN,
            chess_engine.PieceID.KING, chess_engine.PieceID.PONTIFF, chess_engine.PieceID.KNIGHT, chess_engine.PieceID.ROOK
        ]

        # Calculate padding needed (e.g., 10x10 needs 2 extra pieces)
        padding_needed = size - 8
        pad_left = padding_needed // 2
        pad_right = padding_needed - pad_left
        
        # Pad the wings with Towers if the board is larger than 8x8
        back_rank = [chess_engine.PieceID.TOWER] * pad_left + base_back_rank + [chess_engine.PieceID.TOWER] * pad_right

        for col, piece in enumerate(back_rank):
            b_idx = self.geo.coord_to_index(col, 0) 
            w_idx = self.geo.coord_to_index(col, size - 1) 
            
            self.board.add_piece(chess_engine.Color.WHITE, piece, w_idx)
            self.board.add_piece(chess_engine.Color.BLACK, piece, b_idx)

        self.board.castling_rights = 15
        self.canvas.update()

    def check_game_state(self):
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        in_check = chess_engine.is_in_check(self.board, self.geo, self.board.turn)
        turn_str = "White" if self.board.turn == chess_engine.Color.WHITE else "Black"

        if len(legal_moves) == 0:
            if in_check:
                self.info_panel.setText(f"🏆 CHECKMATE! {turn_str} has no escape and loses!")
                self.info_panel.setStyleSheet("font-size: 16px; padding: 10px; font-weight: bold; color: #FF4D4D;")
            else:
                self.info_panel.setText(f"🤝 STALEMATE! {turn_str} has no legal moves. It's a draw.")
                self.info_panel.setStyleSheet("font-size: 16px; padding: 10px; font-weight: bold; color: #5DADE2;")
            
            self.canvas.setEnabled(False) 
            return True
        elif in_check:
            self.info_panel.setText(f"⚠️ CHECK! {turn_str}'s Royal is under attack!")
            return False
            
        return False

    def execute_player_move(self, from_idx, to_idx):
        legal_moves = chess_engine.generate_legal_moves(self.board, self.geo)
        matching_moves = [m for m in legal_moves if m.get_from() == from_idx and m.get_to() == to_idx]
        selected_move = None
        
        if len(matching_moves) == 1:
            selected_move = matching_moves[0]
        elif len(matching_moves) > 1:
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
                
        if not selected_move:
            self.info_panel.setText("❌ Illegal Move! Try again.")
            return

        # --- UPDATE HIGHLIGHTS & GRAVEYARD BEFORE EXECUTING ---
        self.canvas.last_move = (from_idx, to_idx)
        
        # THE FIX: Look directly at the board to see what is about to be captured
        enemy_color = chess_engine.Color.BLACK if self.board.turn == chess_engine.Color.WHITE else chess_engine.Color.WHITE
        captured = self.board.get_piece_at(to_idx, enemy_color)
        
        if captured != chess_engine.PieceID.EMPTY:
            if self.board.turn == chess_engine.Color.WHITE:
                self.bottom_graveyard.add_piece(captured)
            else:
                self.top_graveyard.add_piece(captured)

        # Execute the move internally
        chess_engine.execute_move(self.board, self.geo, selected_move)
        self.canvas.update()

        if self.hotseat_mode:
            turn_color = "White" if self.board.turn == chess_engine.Color.WHITE else "Black" 
            self.info_panel.setText(f"Hotseat Mode: {turn_color}'s Turn.")
        else:
            self.info_panel.setText("Black is thinking...")

        if self.check_game_state():
            return

        if self.hotseat_mode:
            turn_color = "White" if self.board.turn == chess_engine.Color.WHITE else "Black" 
            self.info_panel.setText(f"Hotseat Mode: {turn_color}'s Turn.")
        else:
            self.trigger_ai()

    def execute_ai_move(self, move_str, time_taken):
        if not move_str or move_str == "ERROR":
            self.info_panel.setText("AI Crashed or found no moves.")
            self.canvas.setEnabled(True)
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
            # --- UPDATE HIGHLIGHTS & GRAVEYARD BEFORE EXECUTING ---
            self.canvas.last_move = (from_idx, to_idx)
            
            # THE FIX: Look directly at the board to see what the AI is about to capture
            enemy_color = chess_engine.Color.BLACK if self.board.turn == chess_engine.Color.WHITE else chess_engine.Color.WHITE
            captured = self.board.get_piece_at(to_idx, enemy_color)
            
            if captured != chess_engine.PieceID.EMPTY:
                if self.board.turn == chess_engine.Color.WHITE:
                    self.bottom_graveyard.add_piece(captured)
                else:
                    self.top_graveyard.add_piece(captured)

            # Execute the AI's move on the REAL board
            chess_engine.execute_move(self.board, self.geo, selected_move)
            self.canvas.update()
            
            if self.check_game_state():
                return
                
            player_color_str = "Black" if self.play_black else "White"
            self.info_panel.setText(f"AI Moved: {move_str}. {player_color_str}'s Turn.")
            self.canvas.setEnabled(True)
        
    def closeEvent(self, event):
        if hasattr(self, 'worker') and self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()
        event.accept()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Kings and Cadence Chess Engine")
    parser.add_argument('--hotseat', action='store_true', help='Disable AI and play both sides for debugging')
    parser.add_argument('--black', action='store_true', help='Play as Black (AI plays White)')
    parser.add_argument('--depth', type=int, default=4, help='AI Search Depth (default: 4)')
    args = parser.parse_args()

    app = QApplication(sys.argv)
    
    # Determine whether terminal arguments were specified to override menu screen
    has_flags = ('--hotseat' in sys.argv or '--black' in sys.argv or '--depth' in sys.argv)
    
    window = MainWindow(hotseat_mode=args.hotseat, play_black=args.black, ai_depth=args.depth, bypass_menu=has_flags)
    window.show()
    sys.exit(app.exec())