#include "movegen.h"
#include "magic.h"
#include "attacks.h"
#include "types.h"
#include "zobrist.h"
#include "evaluation.h"
#include "nnue.h"
#include <iostream>


void makeMove(Board& board, Move& move, int ply){
    updateAccumulator(board, ply, move);
    // saving stats for undo
    board.history[board.historyIdx++] = {
        board.castlingRights,
        board.enPassantSquare,
        board.halfMoveClock,
        move.captured,
        board.hash,
        board.gamePhase
    };
    //define
    int from = move.StartSquare;
    int to = move.TargetSquare;
    Color side = board.sideToMove;
    Color opp = (side == WHITE) ? BLACK : WHITE;

    // update the bitboard on capture
    int movingPiece = NONE;
    for (int p = 0; p < 6; p++){
        if (getBit(board.pieces[side][p], from)) {movingPiece = p; break;}
    }
    clearBit(board.pieces[side][movingPiece], from);
    togglePieceHash(board, side, movingPiece, from); // clear the hash of moving piece on from square
    

    // captures
    if(move.captured != NONE && !move.isEnpassant){
        clearBit(board.pieces[opp][move.captured], to);
        togglePieceHash(board, opp, move.captured, to); // clear the hash of captured piece on to square
        board.gamePhase -= gamephaseInc[move.captured];
    }
    setBit(board.pieces[side][movingPiece], to);
    togglePieceHash(board, side, movingPiece, to); // hashes in 


    //promotions
    if(move.promoted != NONE){
        clearBit(board.pieces[side][PAWN], to);
        togglePieceHash(board, side, PAWN, to); // clear the hash of pawn on to square
        setBit(board.pieces[side][move.promoted], to);
        togglePieceHash(board, side, move.promoted, to); // hashes in , promoted piece
        board.gamePhase += gamephaseInc[move.promoted] - gamephaseInc[PAWN];
    }

    //enpassant
    if(move.isEnpassant == true && movingPiece == PAWN){
        int captureSq = (side == WHITE) ? (to - 8) : (to + 8);
        clearBit(board.pieces[opp][PAWN], captureSq);
        togglePieceHash(board, opp, PAWN, captureSq); // clear the hash of pawn on captureSq
        board.gamePhase -= gamephaseInc[PAWN];
    }

    //castle
    if(move.isCastle == true){
        if (to == G1){clearBit(board.pieces[side][ROOK], H1); setBit(board.pieces[side][ROOK], F1);
                      togglePieceHash(board, side, ROOK, H1); togglePieceHash(board, side, ROOK, F1);}
        if (to == C8){clearBit(board.pieces[side][ROOK], A8); setBit(board.pieces[side][ROOK], D8);
                      togglePieceHash(board, side, ROOK, A8); togglePieceHash(board, side, ROOK, D8);} 
        if (to == C1){clearBit(board.pieces[side][ROOK], A1); setBit(board.pieces[side][ROOK], D1);
                      togglePieceHash(board, side, ROOK, A1); togglePieceHash(board, side, ROOK, D1);} 
        if (to == G8){clearBit(board.pieces[side][ROOK], H8); setBit(board.pieces[side][ROOK], F8);
                      togglePieceHash(board, side, ROOK, H8); togglePieceHash(board, side, ROOK, F8);}  
    }

    //state updates

    if (movingPiece == KING) {
        if (side == WHITE) board.castlingRights &= ~(WK_CASTLE | WQ_CASTLE);
        if (side == BLACK) board.castlingRights &= ~(BK_CASTLE | BQ_CASTLE);
    }
    if (movingPiece == ROOK) {
        if (from == H1) board.castlingRights &= ~WK_CASTLE;
        if (from == A1) board.castlingRights &= ~WQ_CASTLE;
        if (from == H8) board.castlingRights &= ~BK_CASTLE;
        if (from == A8) board.castlingRights &= ~BQ_CASTLE;
    }

    if (movingPiece == PAWN && (to-from == 16 || from - to ==16)){
        board.enPassantSquare = ( from + to ) / 2;
    } else {
        board.enPassantSquare = 64;
    }     
    

    if (to == H1) board.castlingRights &= ~WK_CASTLE;
    if (to == A1) board.castlingRights &= ~WQ_CASTLE;
    if (to == H8) board.castlingRights &= ~BK_CASTLE;
    if (to == A8) board.castlingRights &= ~BQ_CASTLE;
    

    if (movingPiece == PAWN || move.captured != NONE) {
        board.halfMoveClock = 0;
    } else {
        board.halfMoveClock++;
    }
    board.sideToMove = (side == WHITE) ? BLACK : WHITE; 
    board.updateOccupancy();

    // accumulator init for moving piece king
    if(movingPiece == KING)
        initAccumulator(board, accStack[ply + 1]);
    

    // hash the state values and side to move back in 
    board.hash ^= zobrist.z_side; 
    board.hash ^= zobrist.z_castling[board.castlingRights];
    if (board.enPassantSquare != 64) {
        board.hash ^= zobrist.z_enPassant[board.enPassantSquare % 8];
    }

}


void unMakeMove(Board& board, Move& move){
    board.sideToMove = (board.sideToMove == WHITE) ? BLACK : WHITE; 
    Color side = board.sideToMove;
    Color opp = (side == WHITE) ? BLACK : WHITE;
    BoardState& st = board.history[--board.historyIdx];
    board.castlingRights = st.castlingRights;
    board.enPassantSquare = st.enPassantSquare;
    board.halfMoveClock = st.halfMoveClock;
    board.hash = st.hash;
    board.gamePhase = st.gamePhase;

    int from = move.StartSquare;
    int to = move.TargetSquare;
    
    // revert the played move :>
    int movedPiece = NONE;
    for (int p = 0; p < 6; p++){
        if (getBit(board.pieces[side][p], to)) {movedPiece = p; break;}
    }
    clearBit(board.pieces[side][movedPiece], to);
    setBit(board.pieces[side][movedPiece], from);
    
    //revert the capture
    if(move.captured != NONE && !move.isEnpassant){setBit(board.pieces[opp][move.captured], to);
    }

    //revert the promotion
    if(move.promoted != NONE){
        clearBit(board.pieces[side][move.promoted], from);
        setBit(board.pieces[side][PAWN], from);
    }    
    
    //revert enpassant
    if(move.isEnpassant == true){
        int captureSq = (side == WHITE) ? to - 8 : to + 8;
        setBit(board.pieces[opp][PAWN], captureSq);
    }

    //revert castle
    if(move.isCastle == true){
        if (to == G1){setBit(board.pieces[side][ROOK], H1); clearBit(board.pieces[side][ROOK], F1);}
        if (to == C8){setBit(board.pieces[side][ROOK], A8); clearBit(board.pieces[side][ROOK], D8);} 
        if (to == C1){setBit(board.pieces[side][ROOK], A1); clearBit(board.pieces[side][ROOK], D1);} 
        if (to == G8){setBit(board.pieces[side][ROOK], H8); clearBit(board.pieces[side][ROOK], F8);}  
    }
    //update
    board.updateOccupancy();
}

NullMoveUndo makeNullMove(Board& board) {
    NullMoveUndo undo;
    undo.enPassantSquare = board.enPassantSquare;
    undo.castlingRights  = board.castlingRights;
    undo.halfMoveClock   = board.halfMoveClock;
    undo.hash            = board.hash;

    // remove en passant from hash if set
    if (board.enPassantSquare != 64)
        board.hash ^= zobrist.z_enPassant[board.enPassantSquare % 8];

    board.enPassantSquare = 64;
    board.halfMoveClock++;
    board.hash ^= zobrist.z_side;
    board.sideToMove = (board.sideToMove == WHITE) ? BLACK : WHITE;

    return undo;
}

void unmakeNullMove(Board& board, const NullMoveUndo& undo) {
    board.sideToMove      = (board.sideToMove == WHITE) ? BLACK : WHITE;
    board.enPassantSquare = undo.enPassantSquare;
    board.castlingRights  = undo.castlingRights;
    board.halfMoveClock   = undo.halfMoveClock;
    board.hash            = undo.hash;
}


// helper functions  

bool isKingInCheck(Board& board, Color side){
    Color opp = (side == WHITE) ? BLACK : WHITE;
    int kingSq = getLSB(board.pieces[side][KING]);

    if((knightAttacks[kingSq]                                                         & board.pieces[opp][KNIGHT]) != 0){return true;}
    if((pawnAttacks[side][kingSq]                                                      & board.pieces[opp][PAWN])   != 0){return true;}
    if((kingAttacks[kingSq]                                                           & board.pieces[opp][KING])   != 0){return true;}
    if((rookAttacks(kingSq, board.occupied)                                           & board.pieces[opp][ROOK])   != 0){return true;}
    if((bishopAttacks(kingSq, board.occupied)                                         & board.pieces[opp][BISHOP]) != 0){return true;}
    if(((rookAttacks(kingSq, board.occupied) | bishopAttacks(kingSq, board.occupied)) & board.pieces[opp][QUEEN])  != 0){return true;}

    return false;
}

bool isSquareAttackedBy(Board& board, int sq, Color attackerSide) {
    Color victimSide = (attackerSide == WHITE) ? BLACK : WHITE;
    if ((knightAttacks[sq]                                                     & board.pieces[attackerSide][KNIGHT]) != 0) return true;
    if ((pawnAttacks[victimSide][sq]                                           & board.pieces[attackerSide][PAWN]) != 0) return true;
    if ((kingAttacks[sq]                                                       & board.pieces[attackerSide][KING]) != 0) return true;
    if ((rookAttacks(sq, board.occupied)                                       & board.pieces[attackerSide][ROOK]) != 0) return true;
    if ((bishopAttacks(sq, board.occupied)                                     & board.pieces[attackerSide][BISHOP]) != 0) return true;
    if (((rookAttacks(sq, board.occupied) | bishopAttacks(sq, board.occupied)) & board.pieces[attackerSide][QUEEN]) != 0) return true;

    return false;
}

PieceType getCaptured(Board& board, Color opp, int sq){
    for(int p = 0; p < 6; p++){
        if(getBit(board.pieces[opp][p], sq) != 0) return (PieceType)p;
    }
    return NONE;
}

bool isPromotionSquare(Color side, int sq){
    if(side == WHITE && sq / 8 == 7) return true;
    if(side == BLACK && sq / 8 == 0) return true;
    return false;
}

bool hasNonPawnMaterial(Board& board, Color side) {
    return board.pieces[side][KNIGHT] | board.pieces[side][BISHOP] |
           board.pieces[side][ROOK]   | board.pieces[side][QUEEN];
}


MoveList generateLegalMoves(Board& board, GenMode mode){
    MoveList pseudoList;

    Color side = board.sideToMove;
    Color opp = (side == WHITE) ? BLACK : WHITE;
    U64 friendlyPieces = (side == WHITE) ? board.occupied_white : board.occupied_black;
    U64 opponentPieces = (side == WHITE) ? board.occupied_black : board.occupied_white;
    U64 knights = board.pieces[side][KNIGHT];
    U64 bishops = board.pieces[side][BISHOP];
    U64 rooks = board.pieces[side][ROOK];
    U64 pawns = board.pieces[side][PAWN];
    U64 kings = board.pieces[side][KING];
    U64 queens = board.pieces[side][QUEEN];
    U64 rank2 = 0x000000000000FF00ULL;
    U64 rank7 = 0x00FF000000000000ULL;
// knights
    while(knights){
        int sq = popLSB(knights);
        U64 attacks = knightAttacks[sq] & ~friendlyPieces;
        while (attacks){
            int attackSq = popLSB(attacks);
            PieceType captured = getCaptured(board, opp, attackSq);
            pseudoList.add({sq, attackSq, captured, NONE, false, false});
        }
    }
// bishops
    while(bishops){
        int sq = popLSB(bishops);
        U64 attacks = bishopAttacks(sq, board.occupied) & ~friendlyPieces;
        while (attacks){
            int attackSq = popLSB(attacks);
            PieceType captured = getCaptured(board, opp, attackSq);
            pseudoList.add({sq, attackSq, captured, NONE, false, false});
        }
    }
// rooks
    while(rooks){
        int sq = popLSB(rooks);
        U64 attacks = rookAttacks(sq, board.occupied) & ~friendlyPieces;
        while (attacks){
            int attackSq = popLSB(attacks);
            PieceType captured = getCaptured(board, opp, attackSq);
            pseudoList.add({sq, attackSq, captured, NONE, false, false});
        }
    }
//queens
    while(queens){
        int sq = popLSB(queens);
        U64 attacks = (rookAttacks(sq, board.occupied) | bishopAttacks(sq, board.occupied)) & ~friendlyPieces;
        while (attacks){
            int attackSq = popLSB(attacks);
            PieceType captured = getCaptured(board, opp, attackSq);
            pseudoList.add({sq, attackSq, captured, NONE, false, false});
        }
    }
//pawns
    U64 pawnCopies = pawns;
    while(pawnCopies){
        int sq = popLSB(pawnCopies);
        U64 epTarget = (board.enPassantSquare != 64) ? (1ULL << board.enPassantSquare) : 0ULL;
        U64 attacks = pawnAttacks[side][sq] & (opponentPieces | epTarget);    
        while (attacks){
            int attackSq = popLSB(attacks);
            bool enpassant = (attackSq == board.enPassantSquare);
            PieceType captured = enpassant ? PAWN : getCaptured(board, opp, attackSq);
            // bool enpassant = (attackSq == board.enPassantSquare);
            if (isPromotionSquare(side, attackSq)) {
                for (PieceType promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                    pseudoList.add({sq, attackSq, captured, promo, enpassant, false});
                }
            } else {
                pseudoList.add({sq, attackSq, captured, NONE, enpassant, false});
            }
        }
    }
    
    // Single Pushes
    U64 singlePush = (side == WHITE) ? (pawns << 8) : (pawns >> 8);
    singlePush &= board.empties;

    while(singlePush){
        int toSq = popLSB(singlePush);
        int fromSq = (side == WHITE) ? toSq - 8 : toSq + 8;
        
        if (isPromotionSquare(side, toSq)) {
            for (PieceType promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                pseudoList.add({fromSq, toSq, NONE, promo, false, false});
            }
        } else {
            pseudoList.add({fromSq, toSq, NONE, NONE, false, false});
        }
    }

    // Double Pushes
        U64 doublePush = 0ULL;
        if (side == WHITE) {
            U64 rank3Squares = (pawns & rank2) << 8;
            rank3Squares &= board.empties; // Must be able to step through rank 3
            doublePush = rank3Squares << 8;
            doublePush &= board.empties; // Rank 4 must be empty too
        } else {
            U64 rank6Squares = (pawns & rank7) >> 8;
            rank6Squares &= board.empties; // Must be able to step through rank 6
            doublePush = rank6Squares >> 8;
            doublePush &= board.empties; // Rank 5 must be empty too
        }

        while(doublePush){
            int toSq = popLSB(doublePush);
            int fromSq = (side == WHITE) ? toSq - 16 : toSq + 16;
            pseudoList.add({fromSq, toSq, NONE, NONE, false, false});
    }
//kings
    while(kings){
        int sq = popLSB(kings);
        U64 attacks = kingAttacks[sq] & ~friendlyPieces;
        while (attacks){
            int attackSq = popLSB(attacks);
            PieceType captured = getCaptured(board, opp, attackSq);
            pseudoList.add({sq, attackSq, captured, NONE, false, false});
        }
    }


    if (!isKingInCheck(board, side)) {
        if (side == WHITE) {
            // White Kingside (E1 to G1)
            bool castling_possible_WK =
                        (board.castlingRights & WK_CASTLE) && 
                        (!getBit(board.occupied, F1) && !getBit(board.occupied, G1)) &&
                        (!isSquareAttackedBy(board, F1, BLACK)) && (!isSquareAttackedBy(board, G1, BLACK));

            if (castling_possible_WK) {
                pseudoList.add({E1, G1, NONE, NONE, false, true});
            }

            // White Queenside (E1 to C1)
            bool castling_possible_WQ =
                        (board.castlingRights & WQ_CASTLE) && 
                        (!getBit(board.occupied, B1) && !getBit(board.occupied, C1) && !getBit(board.occupied, D1)) &&
                        (!isSquareAttackedBy(board, D1, BLACK)) && (!isSquareAttackedBy(board, C1, BLACK));

            if (castling_possible_WQ) {
                pseudoList.add({E1, C1, NONE, NONE, false, true});
            }
        } 
        else { // side == BLACK
            // Black Kingside (E8 to G8)
            bool castling_possible_BK =
                        (board.castlingRights & BK_CASTLE) && 
                        (!getBit(board.occupied, F8) && !getBit(board.occupied, G8)) &&
                        (!isSquareAttackedBy(board, F8, WHITE)) && (!isSquareAttackedBy(board, G8, WHITE));

            if (castling_possible_BK) {
                pseudoList.add({E8, G8, NONE, NONE, false, true});
            }

            // Black Queenside (E8 to C8)
            bool castling_possible_BQ =
                        (board.castlingRights & BQ_CASTLE) && 
                        (!getBit(board.occupied, B8) && !getBit(board.occupied, C8) && !getBit(board.occupied, D8)) &&
                        (!isSquareAttackedBy(board, D8, WHITE)) && (!isSquareAttackedBy(board, C8, WHITE));

            if (castling_possible_BQ) {
                pseudoList.add({E8, C8, NONE, NONE, false, true});
            }
        }
    }

    MoveList legalMoveList;
    for(int i = 0; i < pseudoList.count; i++){
        Move MoveToVerify = pseudoList.moves[i]; 

        if(mode == onlyCaptures && MoveToVerify.captured == NONE && MoveToVerify.promoted == NONE && !MoveToVerify.isEnpassant) continue;

        makeMove(board, MoveToVerify);
        bool inCheck = isKingInCheck(board, side);
        unMakeMove(board, MoveToVerify);

        if (!inCheck) {
            legalMoveList.add(MoveToVerify);
        }
    }
    return legalMoveList;
}
