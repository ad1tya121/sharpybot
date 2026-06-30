import torch
import torch.nn as nn

class NNUE(nn.Module):
    def __init__(self):
        super().__init__()

        self.ft = nn.Linear(40960, 256) #feature transformer
        self.l1 = nn.Linear(512, 32)    # hidden layer 1
        self.l2 = nn.Linear(32, 32)     # hidden layer 2
        self.l3 = nn.Linear(32, 1)      # output

    def forward(self, white_features, black_features):
        w = torch.clamp(self.ft(white_features), 0, 1)
        b = torch.clamp(self.ft(black_features), 0, 1)

        features = torch.cat([w, b], dim=1)

        x = torch.clamp(self.l1(features), 0, 1)
        x = torch.clamp(self.l2(x), 0, 1)
        
        return self.l3(x)
    
