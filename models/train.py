from model import NNUE
from dataset import ChessDataset
from torch.utils.data import Dataset, DataLoader
import torch
import torch.nn as nn
import math
import os

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
loss = nn.MSELoss()

if __name__ == '__main__':
    model = NNUE().to(device)
    if os.path.exists('./model.pt'):
        model.load_state_dict(torch.load('./model.pt', weights_only=True))
        print("Resumed from checkpoint")

    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=5, gamma=0.5)


    dataset = ChessDataset(r"./lichess_db_eval.jsonl.zst", max_positions=3000000)
    dataloader = DataLoader(dataset=dataset, batch_size=1024, shuffle=True, num_workers=2)

    # training loop
    num_epoch = 40
    for epoch in range(num_epoch):
        for white_values, black_values, score in dataloader:
            white_values = white_values.to(device)
            black_values = black_values.to(device)
            score = score.to(device)

            #zero gradiants
            optimizer.zero_grad()

            #forward pass
            score_pred = model(white_values, black_values)

            #loss 
            l = loss(score, score_pred)

            # gradiants
            l.backward()

            #update weights
            optimizer.step()
            
            print(f'epoch {epoch+1}, loss {l.item():.4f}')

        scheduler.step()
        torch.save(model.state_dict(), './model.pt')
        print("Saved checkpoint")
   

