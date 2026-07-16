from model import NNUE
from dataset import ChessDataset
from torch.utils.data import Dataset, DataLoader
import torch
import torch.nn as nn
import os

if __name__ == '__main__':
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

    dataset = ChessDataset("lichess-evals.csv")
    dataloader = DataLoader(dataset, batch_size=2048, shuffle=True, num_workers=2)

    model = NNUE().to(device)

    if os.path.exists("model.pt"):
        model.load_state_dict(torch.load("model.pt", weights_only=True))
        print("Resumed from checkpoint")
    else:
        print("Saved Weights not found, creating new model.pt")

    loss = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.00005)


    num_epoch = 40
    
    for epoch in range(num_epoch):
        totalLoss = 0.0
        for b_idx, (features, score) in enumerate(dataloader):
            features = features.to(device)
            score = score.to(device)

            #forward pass
            score_pred = model(features)

            #loss 
            l = loss(score_pred, score)

            #zero gradiants
            optimizer.zero_grad()

            # gradiants
            l.backward()

            #update weights
            optimizer.step()
            
            totalLoss += l.item()
            
            if b_idx % 10 == 0:
                print(f"epoch {epoch+1} | batch {b_idx}/{len(dataloader)} | loss: {l.item():.4f}")
                
        print(f"epoch {epoch+1} | loss: {totalLoss / len(dataloader):.5f}")
        torch.save(model.state_dict(), "model.pt")
   
