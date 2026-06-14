from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPainter, QColor, QFont, QPen
from PyQt6.QtCore import Qt, pyqtSignal, QRect
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

        for row in range(self.geo.active_height):
            for col in range(self.geo.active_width):
                # 1. Coordinate Logic
                idx = self.geo.coord_to_index(col, row)
                
                # Apply flip math: map (col, row) to visual grid
                v_col = (self.geo.active_width - 1 - col) if self.flip_board else col
                v_row = (self.geo.active_height - 1 - row) if self.flip_board else row
                
                x, y = int(v_col * sq_width), int(v_row * sq_height)

                # 2. Draw Squares & Highlights
                is_light = (row + col) % 2 == 0
                bg_color = self.light_color if is_light else self.dark_color
                if idx == self.selected_idx: bg_color = self.select_color
                
                painter.fillRect(x, y, int(sq_width), int(sq_height), bg_color)

                # --- 3. DRAW TRANSPARENT HIGHLIGHTS ---
                
                # --- 3. DRAW TRANSPARENT HIGHLIGHTS ---
                
                # A. Red Check Highlight (Optimized for multiple Royal types)
                if chess_engine.is_in_check(self.board,self.geo,self.board.turn):
                    # Check for ALL royal pieces of the current turn
                    # (Add more PieceIDs here if you make the Regent/others royal)
                    is_royal = (self.board.get_piece_at(idx, self.board.turn) == chess_engine.PieceID.KING)
                    
                    if is_royal:
                        painter.setBrush(QColor(255, 0, 0, 180))
                        painter.setPen(Qt.PenStyle.NoPen)
                        # Optional: Draw a subtle rounded rect instead of full fill
                        painter.drawRoundedRect(x + 4, y + 4, int(sq_width - 8), int(sq_height - 8), 10, 10)

                # B. Legal Move Dots
                if idx in legal_destinations:
                    painter.setBrush(QColor(100, 200, 255, 150))
                    painter.setPen(Qt.PenStyle.NoPen)
                    
                    # Logic: If the destination is occupied, draw a 'ring' instead of a solid dot
                    if self.board.get_piece_at(idx, chess_engine.Color.WHITE) != chess_engine.PieceID.EMPTY or \
                       self.board.get_piece_at(idx, chess_engine.Color.BLACK) != chess_engine.PieceID.EMPTY:
                        # Draw a ring for captures
                        painter.setBrush(Qt.BrushStyle.NoBrush)
                        painter.setPen(QPen(QColor(100, 200, 255, 150), 6))
                        painter.drawEllipse(int(x + sq_width/6), int(y + sq_height/6), int(sq_width*2/3), int(sq_height*2/3))
                    else:
                        # Draw a solid dot for empty squares
                        painter.drawEllipse(int(x + sq_width/3), int(y + sq_height/3), int(sq_width/3), int(sq_height/3))


                # 4. Draw Pieces
                w_piece = self.board.get_piece_at(idx, chess_engine.Color.WHITE)
                b_piece = self.board.get_piece_at(idx, chess_engine.Color.BLACK)
                piece = w_piece if w_piece != chess_engine.PieceID.EMPTY else b_piece
                
                if piece != chess_engine.PieceID.EMPTY:
                    color = QColor(255, 255, 255) if w_piece != chess_engine.PieceID.EMPTY else QColor(0, 0, 0)
                    
                    # Drop shadow for "Depth"
                    painter.setPen(QColor(0, 0, 0, 100))
                    painter.drawText(x+2, y+2, int(sq_width), int(sq_height), Qt.AlignmentFlag.AlignCenter, label_map.get(piece, "?"))
                    
                    # Actual piece
                    painter.setPen(color)
                    painter.drawText(x, y, int(sq_width), int(sq_height), Qt.AlignmentFlag.AlignCenter, label_map.get(piece, "?"))

    def mousePressEvent(self, event):
        sq_width = self.width() / self.geo.active_width
        sq_height = self.height() / self.geo.active_height
        
        col = int(event.position().x() // sq_width)
        row = int(event.position().y() // sq_height)

        # --- THE FLIP MATH (CLICKS) ---
        if self.flip_board:
            col = self.geo.active_width - 1 - col
            row = self.geo.active_height - 1 - row
        
        if 0 <= col < self.geo.active_width and 0 <= row < self.geo.active_height:
            clicked_idx = self.geo.coord_to_index(col, row)
            
            if self.selected_idx is None:
                # Check if we clicked our own piece
                piece = self.board.get_piece_at(clicked_idx, self.board.turn)
                if piece != chess_engine.PieceID.EMPTY:
                    self.selected_idx = clicked_idx
            else:
                if clicked_idx != self.selected_idx:
                    self.move_played.emit(self.selected_idx, clicked_idx)
                self.selected_idx = None 
                
            self.update()