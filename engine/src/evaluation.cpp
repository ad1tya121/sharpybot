#include "board.h"
#include "evaluation.h"


int evaluate(Board& board){
    int mgScore[2] = {0, 0};
    int egScore[2] = {0, 0};
    int gamePhase = 0;

    for(int p = 0; p < 6; p++){
        U64 whitePieces = board.pieces[WHITE][p];
        while(whitePieces) {
            int square = popLSB(whitePieces);
            mgScore[WHITE] += mg_value[p] + mg_pesto_table[p][FLIP(square)];
            egScore[WHITE] += eg_value[p] + eg_pesto_table[p][FLIP(square)];
            gamePhase += gamephaseInc[p];
        }  
        U64 blackPieces = board.pieces[BLACK][p];
        while(blackPieces){
            int square = popLSB(blackPieces);
            mgScore[BLACK] += mg_value[p] + mg_pesto_table[p][square];
            egScore[BLACK] += eg_value[p] + eg_pesto_table[p][square];
            gamePhase += gamephaseInc[p];
        }
    }

    int mgTotal = mgScore[WHITE] - mgScore[BLACK];
    int egTotal = egScore[WHITE] - egScore[BLACK];

    if(gamePhase > 24) gamePhase = 24;
    int score = (  (mgTotal * gamePhase) + (egTotal * (24 - gamePhase))  ) / 24;
    return board.sideToMove == WHITE ? score : -score;
}

