from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPainter, QColor, QFont
from PyQt6.QtCore import Qt
import chess_engine

class GraveyardWidget(QWidget):
    def __init__(self, piece_cache, piece_color):
        super().__init__()
        self.piece_color = piece_color 
        self.captured_pieces = []
        self.setFixedHeight(40)
        
        self.label_map = {
            chess_engine.PieceID.KING: "♔", chess_engine.PieceID.QUEEN: "♕",
            chess_engine.PieceID.ROOK: "♖", chess_engine.PieceID.BISHOP: "♗",
            chess_engine.PieceID.KNIGHT: "♘", chess_engine.PieceID.PAWN: "♙",
            chess_engine.PieceID.PEASANT: "⚯", chess_engine.PieceID.SOLDIER: "S",
            chess_engine.PieceID.BANDIT: "⚔", chess_engine.PieceID.CULVERIN: '🏹',
            chess_engine.PieceID.HARPOONER: '🎣', chess_engine.PieceID.POWDER_KEG: '💣',
            chess_engine.PieceID.TOWER: '♜', chess_engine.PieceID.REGENT: '♛',
            chess_engine.PieceID.JESTER: '🃏', chess_engine.PieceID.PONTIFF: '⚜',
        }

    def add_piece(self, piece_id):
        self.captured_pieces.append(piece_id)
        # THE FIX: Safely extract the integer value from the C++ Pybind11 Enum
        self.captured_pieces.sort(key=lambda x: x.value)
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        font = QFont("Arial", 22, QFont.Weight.Bold)
        painter.setFont(font)
        
        x_offset = 10
        text_color = QColor(255, 255, 255) if self.piece_color == chess_engine.Color.WHITE else QColor(0, 0, 0)
        
        for piece in self.captured_pieces:
            char = self.label_map.get(piece, "?")
            
            painter.setPen(QColor(0, 0, 0, 100))
            painter.drawText(x_offset + 2, 5 + 2, 30, 30, Qt.AlignmentFlag.AlignCenter, char)
            
            painter.setPen(text_color)
            painter.drawText(x_offset, 5, 30, 30, Qt.AlignmentFlag.AlignCenter, char)
            
            if piece == chess_engine.PieceID.PAWN:
                x_offset += 15 
            else:
                x_offset += 26