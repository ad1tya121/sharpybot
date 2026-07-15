import torch
import numpy as np
import pandas as pd
from torch.utils.data import Dataset
import os
import glob

def parse_eval(raw) -> int:
    raw = str(raw).strip()
    if raw.startswith('#'):
        mate_num = int(raw[1:])
        mate_cp = 10000
        return mate_cp if mate_num > 0 else -mate_cp
    return int(raw)

def parse_fen(fen: str):
    parts = fen.split(" ")
    piecePart = parts[0]
    turn = parts[1]
    rank, file = 7, 0
    pieces = {}
    white_king, black_king = -1, -1
    for c in piecePart:
        if c == '/':
            rank -= 1
            file = 0
        elif c.isdigit():
            file += int(c)
        else:
            color = 0 if c.isupper() else 1
            pt = {'p':0, 'n':1, 'b':2, 'r':3, 'q':4, 'k':5}[c.lower()]
            sq = rank * 8 + file
            pieces[sq] = (pt, color)
            file += 1
            if pt == 5 and color == 0: white_king = sq
            if pt == 5 and color == 1: black_king = sq

    side = 0 if turn == 'w' else 1
    return pieces, side, white_king, black_king

def get_halfKp_indices(pieces, white_king, black_king):
    white_indices = []
    black_indices = []

    mirrored_black_king = black_king ^ 56  # flip rank for black's perspective

    for sq, (pt, color) in pieces.items():
        if pt == 5:
            continue

        # White perspective (unchanged)
        p_idx_white = pt * 2 + color
        white_index = sq + (p_idx_white + white_king * 10) * 64
        white_indices.append(white_index)

        # Black perspective: mirror square, flip color
        mirrored_sq = sq ^ 56
        flipped_color = 1 - color
        p_idx_black = pt * 2 + flipped_color
        black_index = mirrored_sq + (p_idx_black + mirrored_black_king * 10) * 64
        black_indices.append(black_index)

    return white_indices, black_indices

class ChessDataset(Dataset):
    def __init__(self, file_path, max_positions=None):
        super().__init__()
        self.data = []
        self._load(file_path, max_positions)

    def _load(self, file_path, max_positions=None):
        csv_files = glob.glob(os.path.join(file_path, "*.csv"))
        for csv_file in csv_files:
            df = pd.read_csv(csv_file)
            for fen, raw_eval in zip(df['FEN'], df['Evaluation']):
                try:
                    cp = parse_eval(raw_eval)
                except ValueError:
                    continue

                # clip extreme scores
                cp = max(-2000, min(2000, cp))

                pieces, side, white_king, black_king = parse_fen(fen)
                w_idx, b_idx = get_halfKp_indices(pieces, white_king, black_king)

                mover_relative_cp = cp if side == 0 else -cp
                score = 1.0 / (1.0 + np.exp(-mover_relative_cp / 400.0))

                self.data.append((w_idx, b_idx, side, score))                
                if max_positions and len(self.data) >= max_positions:
                    return

    def __len__(self):
        return len(self.data)

    def __getitem__(self, index):
        w_idx, b_idx, side, score = self.data[index]

        white_tensor = torch.zeros(40960)
        black_tensor = torch.zeros(40960)

        if w_idx: white_tensor[w_idx] = 1.0
        if b_idx: black_tensor[b_idx] = 1.0

        if side == 0:  # white to move → us=white, them=black
            us_tensor, them_tensor = white_tensor, black_tensor
        else:          # black to move → us=black, them=white
            us_tensor, them_tensor = black_tensor, white_tensor

        return us_tensor, them_tensor, torch.tensor([score], dtype=torch.float32)