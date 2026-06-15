from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPainter, QColor, QFont, QPen
from PyQt6.QtCore import Qt, pyqtSignal, QRect, QPoint
import chess_engine

class BoardCanvas(QWidget):
    move_played = pyqtSignal(int, int)

    def __init__(self, geometry, board_state, flip_board=False):
        super().__init__()
        self.geo = geometry
        self.board = board_state
        self.flip_board = flip_board  # Store the flag
        self.setMinimumSize(400, 400) 
        
        self.light_color = QColor(240, 217, 181)
        self.dark_color = QColor(181, 136, 99)
        self.select_color = QColor(130, 151, 105, 180) 
        
        # --- NEW VARIABLES ---
        self.last_move = None  # Will hold a tuple like (from_idx, to_idx)
        self.dragging = False
        self.drag_pos = QPoint()
        self.selected_idx = None

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        sq_width = self.width() / self.geo.active_width
        sq_height = self.height() / self.geo.active_height

        # Use a high-quality font for the best possible "text" look
        font = QFont("Arial", int(sq_width * 0.5), QFont.Weight.Bold)
        painter.setFont(font)

        label_map = {
            chess_engine.PieceID.KING: "♔", chess_engine.PieceID.QUEEN: "♕",
            chess_engine.PieceID.ROOK: "♖", chess_engine.PieceID.BISHOP: "♗",
            chess_engine.PieceID.KNIGHT: "♘", chess_engine.PieceID.PAWN: "♙",
            chess_engine.PieceID.PEASANT: "⚯", chess_engine.PieceID.SOLDIER: "S",
            chess_engine.PieceID.BANDIT: "⚔", chess_engine.PieceID.CULVERIN: '🏹',
            chess_engine.PieceID.HARPOONER: '🎣', chess_engine.PieceID.POWDER_KEG: '💣',
            chess_engine.PieceID.TOWER: '♜', chess_engine.PieceID.REGENT: '♛',
            chess_engine.PieceID.JESTER: '🃏', chess_engine.PieceID.PONTIFF: '⚜',
        }

        # Pre-compute legal moves for the selected piece
        legal_destinations = set()
        if self.selected_idx is not None and self.selected_idx != -1:
            all_moves = chess_engine.generate_legal_moves(self.board, self.geo)
            for m in all_moves:
                if m.get_from() == self.selected_idx:
                    legal_destinations.add(m.get_to())

        # Variables to track the piece currently floating under the mouse
        dragged_piece = None
        dragged_is_white = False

        for row in range(self.geo.active_height):
            for col in range(self.geo.active_width):
                # 1. Coordinate Logic
                idx = self.geo.coord_to_index(col, row)
                
                # Apply flip math: map (col, row) to visual grid
                v_col = (self.geo.active_width - 1 - col) if self.flip_board else col
                v_row = (self.geo.active_height - 1 - row) if self.flip_board else row
                
                x, y = int(v_col * sq_width), int(v_row * sq_height)

                # 2. Draw Base Squares
                is_light = (row + col) % 2 == 0
                bg_color = self.light_color if is_light else self.dark_color
                if idx == self.selected_idx: bg_color = self.select_color
                
                painter.fillRect(x, y, int(sq_width), int(sq_height), bg_color)

                # --- NEW: DRAW YELLOW LAST MOVE TINT ---
                if self.last_move and idx in self.last_move:
                    # A soft, semi-transparent yellow
                    painter.fillRect(x, y, int(sq_width), int(sq_height), QColor(255, 255, 50, 100))

                # --- 3. DRAW TRANSPARENT HIGHLIGHTS ---
                
                # A. Red Check Highlight (Now softer and blends with the square)
                if chess_engine.is_in_check(self.board, self.geo, self.board.turn):
                    is_royal = (self.board.get_piece_at(idx, self.board.turn) == chess_engine.PieceID.KING)
                    if is_royal:
                        # Soft red that completely fills the square, tinting the base color
                        painter.fillRect(x, y, int(sq_width), int(sq_height), QColor(225, 75, 75, 130))

                # B. Legal Move Dots
                if idx in legal_destinations:
                    painter.setBrush(QColor(100, 200, 255, 150))
                    painter.setPen(Qt.PenStyle.NoPen)
                    
                    if self.board.get_piece_at(idx, chess_engine.Color.WHITE) != chess_engine.PieceID.EMPTY or \
                       self.board.get_piece_at(idx, chess_engine.Color.BLACK) != chess_engine.PieceID.EMPTY:
                        painter.setBrush(Qt.BrushStyle.NoBrush)
                        painter.setPen(QPen(QColor(100, 200, 255, 150), 6))
                        painter.drawEllipse(int(x + sq_width/6), int(y + sq_height/6), int(sq_width*2/3), int(sq_height*2/3))
                    else:
                        painter.drawEllipse(int(x + sq_width/3), int(y + sq_height/3), int(sq_width/3), int(sq_height/3))

                # 4. Draw Pieces
                w_piece = self.board.get_piece_at(idx, chess_engine.Color.WHITE)
                b_piece = self.board.get_piece_at(idx, chess_engine.Color.BLACK)
                piece = w_piece if w_piece != chess_engine.PieceID.EMPTY else b_piece
                
                if piece != chess_engine.PieceID.EMPTY:
                    # --- NEW: HIDE THE DRAGGED PIECE FROM THE BOARD ---
                    if self.dragging and idx == self.selected_idx:
                        dragged_piece = piece
                        dragged_is_white = (w_piece != chess_engine.PieceID.EMPTY)
                        continue # Skip drawing it on the square!

                    color = QColor(255, 255, 255) if w_piece != chess_engine.PieceID.EMPTY else QColor(0, 0, 0)
                    
                    # Drop shadow for "Depth"
                    painter.setPen(QColor(0, 0, 0, 100))
                    painter.drawText(x+2, y+2, int(sq_width), int(sq_height), Qt.AlignmentFlag.AlignCenter, label_map.get(piece, "?"))
                    # Actual piece
                    painter.setPen(color)
                    painter.drawText(x, y, int(sq_width), int(sq_height), Qt.AlignmentFlag.AlignCenter, label_map.get(piece, "?"))

        # --- 5. DRAW THE FLOATING DRAGGED PIECE ---
        if self.dragging and dragged_piece is not None:
            # Render text slightly larger when dragging
            drag_font = QFont("Arial", int(sq_width * 0.6), QFont.Weight.Bold)
            painter.setFont(drag_font)
            
            color = QColor(255, 255, 255) if dragged_is_white else QColor(0, 0, 0)
            
            # Center the text exactly on the mouse cursor
            draw_x = int(self.drag_pos.x() - sq_width/2)
            draw_y = int(self.drag_pos.y() - sq_height/2)
            
            painter.setPen(QColor(0, 0, 0, 120)) # Stronger drop shadow while dragging
            painter.drawText(draw_x+3, draw_y+3, int(sq_width), int(sq_height), Qt.AlignmentFlag.AlignCenter, label_map.get(dragged_piece, "?"))
            
            painter.setPen(color)
            painter.drawText(draw_x, draw_y, int(sq_width), int(sq_height), Qt.AlignmentFlag.AlignCenter, label_map.get(dragged_piece, "?"))

    def _get_idx_from_pos(self, pos):
        """Helper function to calculate index safely."""
        sq_width = self.width() / self.geo.active_width
        sq_height = self.height() / self.geo.active_height
        
        col = int(pos.x() // sq_width)
        row = int(pos.y() // sq_height)

        if self.flip_board:
            col = self.geo.active_width - 1 - col
            row = self.geo.active_height - 1 - row
            
        if 0 <= col < self.geo.active_width and 0 <= row < self.geo.active_height:
            return self.geo.coord_to_index(col, row)
        return -1

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            idx = self._get_idx_from_pos(event.position())
            if idx == -1: return

            # Click-Click Movement
            if self.selected_idx is not None and idx != self.selected_idx:
                w = self.board.get_piece_at(self.selected_idx, chess_engine.Color.WHITE)
                b = self.board.get_piece_at(self.selected_idx, chess_engine.Color.BLACK)
                if w != chess_engine.PieceID.EMPTY or b != chess_engine.PieceID.EMPTY:
                    self.move_played.emit(self.selected_idx, idx)
                    self.selected_idx = None
                    self.update()
                    return

            # Select for dragging
            self.selected_idx = idx
            w = self.board.get_piece_at(idx, chess_engine.Color.WHITE)
            b = self.board.get_piece_at(idx, chess_engine.Color.BLACK)
            
            if w != chess_engine.PieceID.EMPTY or b != chess_engine.PieceID.EMPTY:
                self.dragging = True
                self.drag_pos = event.position()
            self.update()

    def mouseMoveEvent(self, event):
        if self.dragging:
            self.drag_pos = event.position()
            self.update() # Force repaint so piece follows mouse

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton and self.dragging:
            self.dragging = False
            idx = self._get_idx_from_pos(event.position())
            
            # If dragged to a new square, attempt the move
            if idx != -1 and idx != self.selected_idx:
                self.move_played.emit(self.selected_idx, idx)
                self.selected_idx = None
            
            self.update() # Snap back to grid
        if event.button() == Qt.MouseButton.LeftButton and self.dragging:
            self.dragging = False
            idx = self._get_idx_from_pos(event.position())
            
            # If dragged to a new square, attempt the move
            if idx != -1 and idx != self.selected_idx:
                self.move_played.emit(self.selected_idx, idx)
                self.selected_idx = None
            
            self.update() # Snap back to grid