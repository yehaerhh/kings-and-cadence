from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPainter, QColor, QFont
from PyQt6.QtCore import Qt, pyqtSignal
import chess_engine


class BoardCanvas(QWidget):
    # 235. Signal to alert the Main UI when a complete move is attempted
    # Emits: (from_index, to_index)
    move_played = pyqtSignal(int, int)

    def __init__(self, geometry, board_state):
        super().__init__()
        self.geo = geometry
        self.board = board_state
        self.setMinimumSize(400, 400) 
        
        # Theme Customization
        self.light_color = QColor(240, 217, 181)
        self.dark_color = QColor(181, 136, 99)
        self.select_color = QColor(130, 151, 105, 180) # Semi-transparent green selection
        
        # 233. Selection Tracking States
        self.selected_idx = None
        self.pieces = {} # Mirror dict: { board_index: (color_constant, piece_id_constant) }

    def set_piece_mirror(self, index, color, piece_id):
        """Syncs the visual canvas registry with the underlying C++ state."""
        if color is None:
            self.pieces.pop(index, None)
        else:
            self.pieces[index] = (color, piece_id)
        self.update() # Force redraw

    def clear_mirror(self):
        self.pieces.clear()
        self.selected_idx = None
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        sq_width = self.width() / self.geo.active_width
        sq_height = self.height() / self.geo.active_height

        # 1. Draw Core Dynamic Board Grid
        for row in range(self.geo.active_height):
            for col in range(self.geo.active_width):
                is_light = (row + col) % 2 == 0
                idx = self.geo.coord_to_index(col, row)
                
                bg_color = self.light_color if is_light else self.dark_color
                
                # Highlight if this square is currently selected
                if idx == self.selected_idx:
                    bg_color = self.select_color
                    
                painter.fillRect(
                    int(col * sq_width), int(row * sq_height), 
                    int(sq_width), int(sq_height), bg_color
                )

        # 2. Render Faction Archetypes (Task 232)
        font = QFont("sans-serif", 24, QFont.Weight.Bold)
        painter.setFont(font)

        # Map internal engine IDs to clean minimalist labels
        label_map = {
            chess_engine.KING: "♚",
            chess_engine.QUEEN: "♛",
            chess_engine.ROOK: "♜",
            chess_engine.BISHOP: "♝",
            chess_engine.KNIGHT: "♞",
            chess_engine.PEASANT: "♟", # Using Peasant as the standard Pawn
            chess_engine.BANDIT: "🥷"
        }

        for idx, (color, piece_id) in self.pieces.items():
            # Convert the flat 1D board index back to screen coordinates
            # Because index_to_coord returns a tuple (x, y), we unpack it directly
            col, row = self.geo.index_to_coord(idx)
            
            # Center the unicode archetype inside the square bounding box
            rect_x = int(col * sq_width)
            rect_y = int(row * sq_height)
            
            # Set font color based on faction alignment (0: WHITE, 1: BLACK)
            text_color = QColor(255, 255, 255) if color == 0 else QColor(20, 20, 20)
            painter.setPen(text_color)
            
            display_char = label_map.get(piece_id, "?")
            painter.drawText(
                rect_x, rect_y, int(sq_width), int(sq_height),
                Qt.AlignmentFlag.AlignCenter, display_char
            )

    # 233 & 234. Handle Target Identification via Screen Coordinates
    def mousePressEvent(self, event):
        sq_width = self.width() / self.geo.active_width
        sq_height = self.height() / self.geo.active_height
        
        col = int(event.position().x() // sq_width)
        row = int(event.position().y() // sq_height)
        
        if 0 <= col < self.geo.active_width and 0 <= row < self.geo.active_height:
            # Let the C++ bridge handle the internal stride math!
            clicked_idx = self.geo.coord_to_index(col, row)
            
            if self.selected_idx is None:
                if clicked_idx in self.pieces:
                    piece_color, piece_id = self.pieces[clicked_idx]
                    
                    # THE FIX: Only allow selecting pieces that belong to the current turn!
                    # (This stops you from accidentally grabbing black pawns on White's turn)
                    if piece_color == self.board.turn:
                        self.selected_idx = clicked_idx
            else:
                if clicked_idx != self.selected_idx:
                    self.move_played.emit(self.selected_idx, clicked_idx)
                self.selected_idx = None # Clear selection state
                
            self.update()