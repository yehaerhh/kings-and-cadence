import sys
import os

# We need to tell Python where to find your compiled C++ library!
# It looks in the build directory up one level.
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'build')))
import chess_engine

from PyQt6.QtCore import QThread, pyqtSignal

# 217. Isolate C++ AI calculations in a background worker
class EngineWorker(QThread):
    # 218. Define Signals to talk back to the Main UI Thread
    # We will emit: (Move String, Time Taken)
    result_ready = pyqtSignal(str, float)
    
    # 219. Signal for depth progress (we'll implement this later)
    # depth_progress = pyqtSignal(int) 

    def __init__(self, board_state, geometry, search_depth):
        super().__init__()
        self.board = board_state
        self.geo = geometry
        self.depth = search_depth
        
        # 221. Atomic flag to allow interrupting the thread
        self.is_running = True 

    # 220. The loop that runs in the background
    def run(self):
        import time
        start = time.time()
        
        try:
            # We ONLY calculate if the stop button hasn't been pressed
            if self.is_running:
                # Call the C++ Engine!
                best_move = chess_engine.get_best_move(self.board, self.geo, self.depth)
                
                # We survived the C++ void. Format the result.
                move_str = best_move.to_string(self.geo)
                time_taken = time.time() - start
                
                # Emit the signal back to the UI
                self.result_ready.emit(move_str, time_taken)
                
        except RuntimeError as e:
            # Catch the C++ exception we built in Task 214!
            print(f"Engine Crash Caught in Worker: {e}")
            self.result_ready.emit("ERROR", 0.0)
            
    def stop(self):
        """Safely interrupt the worker."""
        self.is_running = False