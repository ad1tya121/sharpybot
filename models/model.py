import torch
import torch.nn as nn

class NNUE(nn.Module):
    def __init__(self):
        super().__init__()

        self.ft = nn.Linear(40960, 256) #feature transformer
        self.l1 = nn.Linear(256, 32)    # hidden layer 1
        self.l2 = nn.Linear(32, 32)     # hidden layer 2
        self.l3 = nn.Linear(32, 1)      # output

    def forward(self, x):
        x = torch.clamp(self.ft(x), 0.0, 1.0)
        x = torch.clamp(self.l1(x), 0.0, 1.0)
        x = torch.clamp(self.l2(x), 0.0, 1.0)
        x = self.l3(x)
        return torch.sigmoid(x)
    
