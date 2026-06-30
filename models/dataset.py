import torch
import numpy as np
from torch.utils.data import Dataset
import zstandard as zstd
import json
import io

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
    for sq, (pt, color) in pieces.items():
        if pt == 5:
            continue
        p_idx = pt * 2 + color
        white_index = sq + (p_idx + white_king * 10) * 64
        black_index = sq + (p_idx + black_king * 10) * 64
        white_indices.append(white_index)
        black_indices.append(black_index)
    return white_indices, black_indices

class ChessDataset(Dataset):
    def __init__(self, file_path, max_positions=5000000):
        super().__init__()
        self.data = []
        self._load(file_path, max_positions)

    def _load(self, file_path, max_positions):
        with open(file_path, 'rb') as f:
            dctx = zstd.ZstdDecompressor()
            reader = dctx.stream_reader(f)
            text_reader = io.TextIOWrapper(reader, encoding='utf-8')
            for line in text_reader:
                data = json.loads(line)
                fen = data['fen']
                try:
                    cp = data['evals'][0]['pvs'][0]['cp']
                except (KeyError, IndexError):
                    continue

                # clip extreme scores
                cp = max(-2000, min(2000, cp))

                pieces, side, white_king, black_king = parse_fen(fen)
                w_idx, b_idx = get_halfKp_indices(pieces, white_king, black_king)

                # flip score for black to move
                score = cp / 600.0 if side == 0 else -cp / 600.0

                self.data.append((w_idx, b_idx, score))
                if len(self.data) >= max_positions:
                    break

    def __len__(self):
        return len(self.data)

    def __getitem__(self, index):
        w_idx, b_idx, score = self.data[index]

        white_tensor = torch.zeros(40960)
        black_tensor = torch.zeros(40960)

        if w_idx: white_tensor[w_idx] = 1.0
        if b_idx: black_tensor[b_idx] = 1.0

        return white_tensor, black_tensor, torch.tensor([score], dtype=torch.float32)
