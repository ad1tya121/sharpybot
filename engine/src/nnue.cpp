#include "nnue.h"
#include "evaluation.h"

Network net;
Accumulator accStack[512];

bool loadNetwork(const char* path){
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    fread(&net, sizeof(Network), 1, f);
    fclose(f);
    return true;
}

int32_t CReLU(int32_t value, int32_t min, int32_t max){
    if (value <= min) return min;
    if (value >= max) return max;
    return value;
}

uint16_t halfKpIndex(int friendly_king_sq, int piece_square, PieceType piece_type, bool is_friendly){
    int p_idx = is_friendly ? piece_type : piece_type + 5;
    uint16_t halfkp_idx = piece_square + (p_idx + friendly_king_sq * 10) * 64;
    return halfkp_idx;
}

void refreshWhiteAcc(const Board& board, Accumulator& acc){

    for(int i = 0; i < H1_SIZE; i++) acc.whiteValues[i] = net.featureT_biases[i];
    
    int white_king_sq = getLSB(board.pieces[WHITE][KING]);

    for(Color color = WHITE; color < 2; color = Color(color + 1)){
        for(PieceType piece = PAWN; piece < 5; piece = PieceType(piece + 1)){
            bool is_friendly = (color == WHITE);
            U64 bb = board.pieces[color][piece];
            while(bb != 0ULL){
                int piece_square = popLSB(bb);
                uint16_t idx = halfKpIndex(white_king_sq, piece_square, piece, is_friendly);
                for(int i = 0; i < H1_SIZE; i++) acc.whiteValues[i] += net.featureT_weights[idx][i];
            }
        }
    }
}

void refreshBlackAcc(const Board& board, Accumulator& acc){

    for(int i = 0; i < H1_SIZE; i++) acc.blackValues[i] = net.featureT_biases[i];
    
    int black_king_sq = (getLSB(board.pieces[BLACK][KING])) ^ 56;

    for(Color color = WHITE; color < 2; color = Color(color + 1)){
        for(PieceType piece = PAWN; piece < 5; piece = PieceType(piece + 1)){
            bool is_friendly = (color == BLACK);
            U64 bb = board.pieces[color][piece];
            while(bb != 0ULL){
                int piece_square = (popLSB(bb)) ^ 56;
                uint16_t idx = halfKpIndex(black_king_sq, piece_square, piece, is_friendly);
                for(int i = 0; i < H1_SIZE; i++) acc.blackValues[i] += net.featureT_weights[idx][i];
            }
        }
    }
}

void initAccumulator(const Board& board, Accumulator& acc){
    refreshWhiteAcc(board, acc);
    refreshBlackAcc(board, acc);
}

int32_t nnueEval(const Board& board, int ply){
    Accumulator& acc = accStack[ply];
    Color side = board.sideToMove;

    int16_t* accu = (side == WHITE) ? acc.whiteValues : acc.blackValues; 

    // feature transformer
    int32_t input[H1_SIZE];
    for(int i = 0; i < H1_SIZE; i++) { 
        input[i] = CReLU(accu[i], 0, SCALE);
    }

    // Hidden layer 1
    int32_t hidden1[H2_SIZE];
    for(int neuron = 0; neuron < H2_SIZE; neuron++){
        int32_t sum = 0;
        for(int i = 0; i < H1_SIZE; i++){
            sum += input[i] * net.hidden1_weights[i][neuron];
        }
        hidden1[neuron] = CReLU(sum / SCALE + net.hidden1_biases[neuron], 0, SCALE);
    }

    // Hidden layer 2
    int32_t hidden2[H3_SIZE];
    for(int neuron = 0; neuron < H3_SIZE; neuron++){
        int32_t sum = 0;
        for(int i = 0; i < H2_SIZE; i++){
            sum += hidden1[i] * net.hidden2_weights[i][neuron];
        }
        hidden2[neuron] = CReLU(sum / SCALE + net.hidden2_biases[neuron], 0, SCALE);
    }

    // Output layer
    int64_t sum3 = 0;
    for(int i = 0; i < H3_SIZE; i++){
        sum3 += (int64_t)hidden2[i] * net.output_weights[i];
    }

    float eval = OUT_SCALE * ((float)sum3 / (SCALE * SCALE) + (float)net.output_bias / SCALE);

    eval = CReLU(eval, -2000.0f, 2000.0f);
    return (int32_t)eval;
}


void updateAccumulator(const Board& board, int ply, Move move){
    Color side = board.sideToMove;
    Color opp = (side==WHITE) ? BLACK : WHITE; 
    int enpassant_sq = (side == WHITE) ? (move.TargetSquare - 8) : (move.TargetSquare + 8);

    int movingPiece = 0;
    for(int p = 0; p < 6; p++){
        if(getBit(board.pieces[side][p], move.StartSquare)) { movingPiece = p; break; }
    }

    if(movingPiece==KING) return;

    bool isCapture   = move.captured != NONE;
    bool isPromotion = move.promoted != NONE;
    bool isEP        = move.isEnpassant;

    // ---- White accumulator ----

    {
        int friendly_king_sq = getLSB(board.pieces[WHITE][KING]);

        bool movedPieceIsFriendly    = (side == WHITE);
        bool capturedPieceIsFriendly = (opp  == WHITE);

        uint16_t idxRemoveFromStart   = halfKpIndex(friendly_king_sq, move.StartSquare,  PieceType(movingPiece),    movedPieceIsFriendly);
        uint16_t idxAddAtTarget       = halfKpIndex(friendly_king_sq, move.TargetSquare, PieceType(movingPiece),    movedPieceIsFriendly);
        uint16_t idxRemoveCaptured    = halfKpIndex(friendly_king_sq, move.TargetSquare, PieceType(move.captured),  capturedPieceIsFriendly);
        uint16_t idxRemovePromoPawn   = halfKpIndex(friendly_king_sq, move.TargetSquare, PAWN,                      movedPieceIsFriendly);
        uint16_t idxAddPromotedPiece  = halfKpIndex(friendly_king_sq, move.TargetSquare, PieceType(move.promoted),  movedPieceIsFriendly);
        uint16_t idxRemoveEpPawn      = halfKpIndex(friendly_king_sq, enpassant_sq,      PAWN,                      capturedPieceIsFriendly);

        for(int i = 0; i < H1_SIZE; i++)
            accStack[ply+1].whiteValues[i] = accStack[ply].whiteValues[i];

        for(int i = 0; i < H1_SIZE; i++){
            accStack[ply+1].whiteValues[i] -= net.featureT_weights[idxRemoveFromStart][i];
            accStack[ply+1].whiteValues[i] += net.featureT_weights[idxAddAtTarget][i];

            if(isCapture && !isEP)
                accStack[ply+1].whiteValues[i] -= net.featureT_weights[idxRemoveCaptured][i];

            if(isPromotion){
                accStack[ply+1].whiteValues[i] -= net.featureT_weights[idxRemovePromoPawn][i];
                accStack[ply+1].whiteValues[i] += net.featureT_weights[idxAddPromotedPiece][i];
            }

            if(isEP)
                accStack[ply+1].whiteValues[i] -= net.featureT_weights[idxRemoveEpPawn][i];
        }


    }

    // ---- Black accumulator ----
    {
        int friendly_king_sq = (getLSB(board.pieces[BLACK][KING])) ^ 56;

        bool movedPieceIsFriendly    = (side == BLACK);
        bool capturedPieceIsFriendly = (opp  == BLACK);

        uint16_t idxRemoveFromStart   = halfKpIndex(friendly_king_sq, move.StartSquare ^ 56,  PieceType(movingPiece),    movedPieceIsFriendly);
        uint16_t idxAddAtTarget       = halfKpIndex(friendly_king_sq, move.TargetSquare ^ 56, PieceType(movingPiece),    movedPieceIsFriendly);
        uint16_t idxRemoveCaptured    = halfKpIndex(friendly_king_sq, move.TargetSquare ^ 56, PieceType(move.captured),  capturedPieceIsFriendly);
        uint16_t idxRemovePromoPawn   = halfKpIndex(friendly_king_sq, move.TargetSquare ^ 56, PAWN,                      movedPieceIsFriendly);
        uint16_t idxAddPromotedPiece  = halfKpIndex(friendly_king_sq, move.TargetSquare ^ 56, PieceType(move.promoted),  movedPieceIsFriendly);
        uint16_t idxRemoveEpPawn      = halfKpIndex(friendly_king_sq, enpassant_sq ^ 56,      PAWN,                      capturedPieceIsFriendly);

        for(int i = 0; i < H1_SIZE; i++)
            accStack[ply+1].blackValues[i] = accStack[ply].blackValues[i];

        for(int i = 0; i < H1_SIZE; i++){
            accStack[ply+1].blackValues[i] -= net.featureT_weights[idxRemoveFromStart][i];
            accStack[ply+1].blackValues[i] += net.featureT_weights[idxAddAtTarget][i];

            if(isCapture && !isEP)
                accStack[ply+1].blackValues[i] -= net.featureT_weights[idxRemoveCaptured][i];

            if(isPromotion){
                accStack[ply+1].blackValues[i] -= net.featureT_weights[idxRemovePromoPawn][i];
                accStack[ply+1].blackValues[i] += net.featureT_weights[idxAddPromotedPiece][i];
            }

            if(isEP)
                accStack[ply+1].blackValues[i] -= net.featureT_weights[idxRemoveEpPawn][i];
        }


    }

}