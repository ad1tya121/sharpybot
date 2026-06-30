#include "evaluation.h"
#include "attacks.h"
#include "movegen.h"
#include "board.h"
#include "search.h"
#include "tt.h"
#include "uci.h"
#include "nnue.h"
#include <iostream>
#include <cstring>
#include <cmath>

std::atomic<bool> stopSearch{false};
long long nodeCount = 0;
TranspositionTable TT(128);
Move killer_moves[2][256];
int history_moves[2][6][64];

void clearSearchTables() {
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
}

void orderMoves(Board& board, MoveList& moves, Move ttBestMove, int ply){
    int scores[256] = {0}; 
    
    Color side = board.sideToMove;
    Color opp = (side == WHITE) ? BLACK : WHITE;
    U64 oppPawnAttacksBB = opponentPawnAttackMap(board, side);
    
    int phase = board.gamePhase;
    if (phase > 24) phase = 24;
    if (phase < 0)  phase = 0;

    for (int i = 0; i < moves.count; i++) {
        Move currentMove = moves.moves[i];

// tt hash move
        if (currentMove == ttBestMove) {
            scores[i] = 2000000; 
            continue;
        }
        
// MVV_LVA captures move
        int egTotal = 0;
        int mgTotal = 0;
        int movePieceType = PAWN;
        for (int p = 0; p < 6; p++){
            if (getBit(board.pieces[side][p], currentMove.StartSquare)) {movePieceType = p; break;}
        }
        int capturePieceType = getCaptured(board, opp, currentMove.TargetSquare);

        // Prioritise capturing opponent's most valuable piece with our least valuable piece
        if(capturePieceType != NONE){
            egTotal += 10 * eg_pesto_table[capturePieceType][pestoSq(currentMove.TargetSquare, opp)] 
                    - eg_pesto_table[movePieceType][pestoSq(currentMove.StartSquare, side)];
            mgTotal += 10 * mg_pesto_table[capturePieceType][pestoSq(currentMove.TargetSquare, opp)] 
                    - mg_pesto_table[movePieceType][pestoSq(currentMove.StartSquare, side)];


            egTotal += 100000;
            mgTotal += 100000;
        }

        // Promoting a pawn is likely to be good
        if(currentMove.promoted != NONE){
            egTotal += eg_pesto_table[currentMove.promoted][pestoSq(currentMove.TargetSquare, side)];
            mgTotal += mg_pesto_table[currentMove.promoted][pestoSq(currentMove.TargetSquare, side)];
        }

        // Penalize moving our pieces to a square attacked by an opponent pawn
        if(getBit(oppPawnAttacksBB, currentMove.TargetSquare)){
            if(movePieceType != PAWN){
                egTotal -= eg_pesto_table[movePieceType][currentMove.StartSquare];
                mgTotal -= mg_pesto_table[movePieceType][currentMove.StartSquare];
            }
        }

        scores[i] = ((mgTotal * phase) + (egTotal * (24 - phase))) / 24;

// score quiet moves
        bool isQuiet = (capturePieceType == NONE && currentMove.promoted == NONE);
        if(isQuiet){
            // score 1st killer move 
                if(currentMove == killer_moves[0][ply]){
                    scores[i] =  900000;
                }
            // score 2nd killer move
                else if(currentMove == killer_moves[1][ply]){
                    scores[i] =  800000;
                }
            // score history moves
                else {
                    scores[i] = std::min(history_moves[side][movePieceType][currentMove.TargetSquare], 700000);
                } 
        }
    }   

    for (int i = 0; i < moves.count; i++) {
        for (int j = i + 1; j < moves.count; j++) {
            if (scores[j] > scores[i]) {
                std::swap(scores[i], scores[j]);
                std::swap(moves.moves[i], moves.moves[j]);
            }
        }
    }
}

int quiescence(Board& board, int alpha, int beta, int ply){
    if ((nodeCount & 2047) == 0 && stopSearch.load()) return 0;
    bool inCheck = isKingInCheck(board, board.sideToMove);
    int best_value = -infinity;

    if(!inCheck){
        // Stand Pat
        int static_eval = nnueEval(board, ply);                      
        best_value = static_eval;
        if( best_value >= beta )
            return best_value;
        if( best_value > alpha )
            alpha = best_value;
    }

    MoveList qmoves = inCheck
        ? generateLegalMoves(board)
        : generateLegalMoves(board, onlyCaptures);

    if (inCheck && qmoves.count == 0) return -mateScore + ply;

    Move blankMove{};
    orderMoves(board, qmoves, blankMove, ply);

    for(int i = 0; i < qmoves.count; i++){
        Move move_in_qmoves = qmoves.moves[i];

        makeMove(board, move_in_qmoves, ply);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        unMakeMove(board, move_in_qmoves);
        
        if(score >= beta) return score;
        if(score > best_value) best_value = score;
        if(score > alpha) alpha = score;
    }

    return best_value;
}


int alphaBeta(Board& board, int alpha, int beta, int depthleft, int ply){
    nodeCount++;
    if ((nodeCount & 2047) == 0 && stopSearch.load()) return 0;
    int originalAlpha = alpha;
    bool inCheck = isKingInCheck(board, board.sideToMove);

    Move bestMove{};
// TT Lookup
    int ttscore;
    if (TT.probeHash(board.hash, depthleft, ttscore, alpha, beta, bestMove)) {
        return scoreFromTT(ttscore, ply);
    }

    if (depthleft == 0) return quiescence(board, alpha, beta, ply);

// NULL MOVE PRUNING
    if (!inCheck && depthleft >= 3 && ply > 0 && hasNonPawnMaterial(board, board.sideToMove)) {
        int R = 3 + depthleft / 4;
        if (depthleft - 1 - R > 0) {  // only null move if reduced depth stays above 0
            NullMoveUndo undo = makeNullMove(board);
            int nullScore = -alphaBeta(board, -beta, -beta + 1, depthleft - 1 - R, ply + 1);
            unmakeNullMove(board, undo);
            if (stopSearch.load()) return 0;
            if (nullScore >= beta) return beta;
        }
    }
// Futility pruning — skip moves that can't possibly raise alpha
    bool fPrune = false;
    int fMargin[4] = {0, 80, 160, 250};
    if (depthleft <= 3 && !inCheck && abs(alpha) < 9000) {
        int staticEval = nnueEval(board, ply);
        if (staticEval + fMargin[depthleft] <= alpha)
            fPrune = true;
    }

    MoveList allMoves = generateLegalMoves(board);
    orderMoves(board, allMoves, bestMove, ply);

    if (allMoves.count == 0) {
        if (inCheck) return -mateScore + ply;
        return 0;
    }
    
    int best_value = -infinity; 

    for (int i = 0; i < allMoves.count; i++) {
        Move current_move = allMoves.moves[i];
        bool wasCapture = (getCaptured(board, (board.sideToMove == WHITE ? BLACK : WHITE), current_move.TargetSquare) != NONE);

        // futility pruning — skip BEFORE makeMove
        if (fPrune && i > 0 && !wasCapture && current_move.promoted == NONE)
            continue;

        makeMove(board, current_move, ply);

        int score;
        if (i == 0) {
            // first move — full window
            score = -alphaBeta(board, -beta, -alpha, depthleft - 1, ply + 1);} 
        else if (i >= 2 && depthleft >= 3 && !inCheck &&
                !wasCapture && current_move.promoted == NONE &&
                current_move != killer_moves[0][ply]) {

                // LMR
                int R = 1 + (int)(log(depthleft) * log(i) / 2.0);
                R = std::max(1, std::min(R, depthleft - 1));
                score = -alphaBeta(board, -alpha - 1, -alpha, depthleft - 1 - R, ply + 1);
                if (score > alpha && score < beta)
                    score = -alphaBeta(board, -beta, -alpha, depthleft - 1, ply + 1);
        } else {

            // PVS null window for all other moves
            score = -alphaBeta(board, -alpha - 1, -alpha, depthleft - 1, ply + 1);
            if (score > alpha && score < beta)
                score = -alphaBeta(board, -beta, -alpha, depthleft - 1, ply + 1);
        }

        unMakeMove(board, current_move);
        
        if(score > best_value) {
            best_value = score;
            bestMove = current_move;
        }
        if(score > alpha) alpha = score;
        if(score >= beta) {
            if (!stopSearch.load())
                TT.storeHash(board.hash, depthleft, scoreToTT(score, ply), HASH_BETA, current_move);

    // killer/history update — quiet moves only
        if (!wasCapture && current_move.promoted == NONE) {
            // killer moves
            if (current_move != killer_moves[0][ply]) {
                killer_moves[1][ply] = killer_moves[0][ply];
                killer_moves[0][ply] = current_move;
            }

            // history moves
            int movePieceType = PAWN;
            for (int p = 0; p < 6; p++){
                if (getBit(board.pieces[board.sideToMove][p], current_move.StartSquare)) { movePieceType = p; break; }
            }
            history_moves[board.sideToMove][movePieceType][current_move.TargetSquare] += depthleft * depthleft;

            if (history_moves[board.sideToMove][movePieceType][current_move.TargetSquare] >= 1'000'000) {
                for (int s = 0; s < 2; s++)
                    for (int p = 0; p < 6; p++)
                        for (int sq = 0; sq < 64; sq++)
                            history_moves[s][p][sq] /= 2;
            }
        }

            return score;}
    }

    if (!stopSearch.load()) {
        uint8_t flag = (best_value <= originalAlpha) ? HASH_ALPHA : HASH_EXACT;
        TT.storeHash(board.hash, depthleft, scoreToTT(best_value, ply), flag, bestMove);
    }
    return best_value;
}
