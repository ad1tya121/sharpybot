#include "nnue.h"
#include "evaluation.h"

Network net;
Accumulator accStack[512];

inline int32_t CReLU(int16_t value, int16_t min, int16_t max){
    if (value <= min) return min;
    if (value >= max) return max;
    return value;
}

uint16_t halfKpIndex(int king_square, int piece_square, PieceType piece_type, Color piece_color){
    int p_idx = piece_type * 2 + piece_color;
    uint16_t halfkp_idx = piece_square + (p_idx + king_square * 10) * 64;
    return halfkp_idx;
}

void initAccumulator(const Board& board, Accumulator& acc){

    for(int i = 0; i < H1_SIZE; i++){
        acc.whiteValues[i] = net.featureT_biases[i];
        acc.blackValues[i] = net.featureT_biases[i];
    }

    int white_king_sq = getLSB(board.pieces[WHITE][KING]);
    int black_king_sq = getLSB(board.pieces[BLACK][KING]);

    for(Color color = WHITE; color < 2; color = Color(color + 1)){
        for(PieceType piece = PAWN; piece < 5; piece = PieceType(piece + 1)){
            U64 bb = board.pieces[color][piece];
            while(bb != 0ULL){
                int piece_square = popLSB(bb);
                uint16_t whitep_idx = halfKpIndex(white_king_sq, piece_square, piece, color);
                for(int i = 0; i < H1_SIZE; i++)
                    acc.whiteValues[i] += net.featureT_weights[whitep_idx][i];
                uint16_t blackp_idx = halfKpIndex(black_king_sq, piece_square, piece, color);
                for(int j = 0; j < H1_SIZE; j++)
                    acc.blackValues[j] += net.featureT_weights[blackp_idx][j];
            }
        }
    }
}

int32_t nnueEval(const Board& board, int ply){
    Accumulator& acc = accStack[ply];
    Color side = board.sideToMove;

    int32_t input[2 * H1_SIZE];
    if(side == WHITE){
        for(int i = 0; i < H1_SIZE; i++)
            input[i] = CReLU(acc.whiteValues[i], 0, SCALE);
        for(int i = 0; i < H1_SIZE; i++)
            input[i + H1_SIZE] = CReLU(acc.blackValues[i], 0, SCALE);
    } else {
        for(int i = 0; i < H1_SIZE; i++)
            input[i] = CReLU(acc.blackValues[i], 0, SCALE);
        for(int i = 0; i < H1_SIZE; i++)
            input[i + H1_SIZE] = CReLU(acc.whiteValues[i], 0, SCALE);
    }

    // Hidden layer 1
    int32_t hidden1[H2_SIZE] = {0};
    for(int i = 0; i < (2 * H1_SIZE); i++){
        for(int neuron = 0; neuron < H2_SIZE; neuron++){
            hidden1[neuron] += input[i] * net.hidden1_weights[i][neuron];
        }
    }
    for(int neuron = 0; neuron < H2_SIZE; neuron++){
        hidden1[neuron] = CReLU(hidden1[neuron] / SCALE + net.hidden1_biases[neuron], 0, SCALE);
    }

    // Hidden layer 2
    int32_t hidden2[H3_SIZE] = {0};
    for(int i = 0; i < H2_SIZE; i++){
        for(int neuron = 0; neuron < H3_SIZE; neuron++){
            hidden2[neuron] += hidden1[i] * net.hidden2_weights[i][neuron];
        }
    }
    for(int neuron = 0; neuron < H3_SIZE; neuron++){
        hidden2[neuron] = CReLU(hidden2[neuron] / SCALE + net.hidden2_biases[neuron], 0, SCALE);
    }

    // Output layer
    int64_t sum_raw3 = 0;
    for(int i = 0; i < H3_SIZE; i++)
        sum_raw3 += (int64_t)hidden2[i] * net.output_weights[i];

    float eval = OUT_SCALE * ((float)sum_raw3 / (SCALE * SCALE) + (float)net.output_bias / SCALE);

    return (int32_t)eval;
}

bool loadNetwork(const char* path){
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    fread(&net, sizeof(Network), 1, f);
    fclose(f);
    return true;
}

void updateAccumulator(const Board& board, int ply, Move move){
    accStack[ply + 1] = accStack[ply];
    
    int white_king_sq = getLSB(board.pieces[WHITE][KING]);
    int black_king_sq = getLSB(board.pieces[BLACK][KING]);
    Color side = board.sideToMove;
    Color opp = (side==WHITE) ? BLACK : WHITE;
    int epCaptureSquare = (side == WHITE) ? move.TargetSquare - 8 : move.TargetSquare + 8;

    int movingPiece = 0;
    for(int p = 0; p < 6; p++){
        if(getBit(board.pieces[side][p], move.StartSquare)) { movingPiece = p; break; }
    }

    if(movingPiece == KING) return; // full refresh handled after makeMove

    // remove from start square
    uint16_t whiteRemove = halfKpIndex(white_king_sq, move.StartSquare, PieceType(movingPiece), side);
    uint16_t blackRemove = halfKpIndex(black_king_sq, move.StartSquare, PieceType(movingPiece), side);
    // add piece to target square
    uint16_t whiteAdd = halfKpIndex(white_king_sq, move.TargetSquare, PieceType(movingPiece), side);
    uint16_t blackAdd = halfKpIndex(black_king_sq, move.TargetSquare, PieceType(movingPiece), side);
    // remove from target square
    uint16_t oppWhiteRemove = halfKpIndex(white_king_sq, move.TargetSquare, move.captured, opp);
    uint16_t oppBlackRemove = halfKpIndex(black_king_sq, move.TargetSquare, move.captured, opp);    
    // add promoted piece on target piece
    uint16_t promoWhiteAdd = halfKpIndex(white_king_sq, move.TargetSquare, move.promoted, side);
    uint16_t promoBlackAdd = halfKpIndex(black_king_sq, move.TargetSquare, move.promoted, side); 
    // remove pawn from enpassant square
    uint16_t enWhiteRemove = halfKpIndex(white_king_sq, epCaptureSquare, PAWN, opp);
    uint16_t enBlackRemove = halfKpIndex(black_king_sq, epCaptureSquare, PAWN, opp);     


    // remove from start square
    for(int i = 0; i < H1_SIZE; i++){
        accStack[ply+1].whiteValues[i] -= net.featureT_weights[whiteRemove][i];
        accStack[ply+1].blackValues[i] -= net.featureT_weights[blackRemove][i];
    }
    // add to target square
    for(int i = 0; i < H1_SIZE; i++){
        accStack[ply+1].whiteValues[i] += net.featureT_weights[whiteAdd][i];
        accStack[ply+1].blackValues[i] += net.featureT_weights[blackAdd][i];
    }
    // remove the opp captured piece from target square
    if(move.captured != NONE){
        for(int i = 0; i < H1_SIZE; i++){
            accStack[ply+1].whiteValues[i] -= net.featureT_weights[oppWhiteRemove][i];
            accStack[ply+1].blackValues[i] -= net.featureT_weights[oppBlackRemove][i];    
        }
    }
    //promotions
    if(move.promoted != NONE){
        // remove pawn from target square (undo the whiteAdd above)
        for(int i = 0; i < H1_SIZE; i++){
            accStack[ply+1].whiteValues[i] -= net.featureT_weights[whiteAdd][i];
            accStack[ply+1].blackValues[i] -= net.featureT_weights[blackAdd][i];
        }
        // add promoted piece
        for(int i = 0; i < H1_SIZE; i++){
            accStack[ply+1].whiteValues[i] += net.featureT_weights[promoWhiteAdd][i];
            accStack[ply+1].blackValues[i] += net.featureT_weights[promoBlackAdd][i];
        }
    }
    //enpassant
    if(move.isEnpassant){
        for(int i = 0; i < H1_SIZE; i++){
            accStack[ply+1].whiteValues[i] -= net.featureT_weights[enWhiteRemove][i];
            accStack[ply+1].blackValues[i] -= net.featureT_weights[enBlackRemove][i];    
        }  
    }
}
