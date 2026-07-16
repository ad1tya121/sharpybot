import torch
from torch.utils.data import Dataset
import numpy as np
import pandas as pd
class ChessDataset(Dataset):
    def __init__(self, file):
        self.data = pd.read_csv(file, names=['FEN', 'Evaluation'], header=0)
 
    def __len__(self):
        return len(self.data)

    def parse_eval(self, raw, white_turn):
        raw = str(raw)
        if raw.startswith('#'):
            mate_num = int(raw[1:])
            mate_cp = 10000
            eval =  mate_cp if mate_num > 0 else -mate_cp
        else:
            eval = int(raw)

        if not white_turn:
            eval = -eval
        
        return 1.0 / (1.0 + np.exp(-eval /400.0))

    def parse_fen_to_halfkp(self, fen):
        parts = fen.split(" ")
        piecePart = parts[0]
        white_turn = (parts[1] == 'w')
        rank, file = 7, 0
        features = np.zeros(40960, dtype=np.float32)
        pieces = {}
        friendly_king = -1
        for c in piecePart:
            if c == '/':
                rank -= 1
                file = 0
            elif c.isdigit():
                file += int(c)
            else:
                white_piece = c.isupper()
                is_friendly = (white_piece == white_turn)

                pt = {'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5}[c.lower()]
                
                sq = rank * 8 + file
                
                if pt == 5 and is_friendly: friendly_king = sq
                else: pieces[sq] = (pt, is_friendly)

                file += 1


        if not white_turn:
            friendly_king ^= 56
    
        for sq, (pt, is_friendly) in pieces.items():
            if pt == 5:
                continue
            correct_persp_sq = sq if white_turn else sq ^ 56
            
            if is_friendly: p_idx = pt
            else: p_idx = pt + 5

            idx = correct_persp_sq + (p_idx + friendly_king * 10) * 64
            features[idx] = 1.0

        return features

    def __getitem__(self, idx):
        row = self.data.iloc[idx]
        fen = row['FEN']
        val = row['Evaluation']

        white_turn = fen.split(' ')[1] == 'w'

        features = self.parse_fen_to_halfkp(fen)
        target = self.parse_eval(val, white_turn)

        return torch.tensor(features), torch.tensor([target], dtype=torch.float32)


if __name__ == "__main__":
    pass

