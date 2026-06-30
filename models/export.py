import torch
import numpy as np
from model import NNUE

def ExportNetwork(model, path):
    with open(path, 'wb') as p:
        # feature transformer 
        featureT_weights = model.ft.weight.detach().T.contiguous().numpy()
        (featureT_weights * 64).astype(np.int16).tofile(p)
        
        featureT_biases = model.ft.bias.detach().numpy()
        (featureT_biases * 64).astype(np.int16).tofile(p)
    
        # hidden layer 1
        hidden1_weights = model.l1.weight.detach().T.contiguous().numpy()
        (hidden1_weights * 64).astype(np.int16).tofile(p)

        hidden1_biases = model.l1.bias.detach().numpy()
        (hidden1_biases * 64).astype(np.int16).tofile(p)

        # hidden layer 2
        hidden2_weights = model.l2.weight.detach().T.contiguous().numpy()
        (hidden2_weights * 64).astype(np.int16).tofile(p)

        hidden2_biases = model.l2.bias.detach().numpy()
        (hidden2_biases * 64).astype(np.int16).tofile(p) 

        # output layer 
        output_weights = model.l3.weight.detach().numpy()
        (output_weights * 64).astype(np.int16).tofile(p)

        output_biases = model.l3.bias.detach().numpy()
        (output_biases * 64).astype(np.int32).tofile(p)

if __name__ == "__main__":
    model = NNUE()
    model.load_state_dict(torch.load('/content/drive/MyDrive/model.pt', weights_only=True))
    ExportNetwork(model, '/content/drive/MyDrive/model.bin')
    print("Exported to model.bin")
