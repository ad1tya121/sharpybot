#include "attacks.h"


// attack tables defined here
U64 knightAttacks[64];
U64 kingAttacks[64];
U64 pawnAttacks[2][64];

// compute functions of knight, king, pawn
U64 computeKnightAttacks(squareIndex square){
    U64 knightBB = 1ULL << square;
    U64 NorthNorthEast = (knightBB & notHFile) << 17;
    U64 NorthNorthWest = (knightBB & notAFile) << 15;
    U64 EastEastNorth = (knightBB & notGHFile) << 10;
    U64 EastEastSouth = (knightBB & notGHFile) >> 6;
    U64 SouthSouthEast = (knightBB & notHFile) >> 15;
    U64 SouthSouthWest = (knightBB & notAFile) >> 17;
    U64 WestWestNorth = (knightBB & notABFile) << 6;
    U64 WestWestSouth = (knightBB & notABFile) >> 10;
    return NorthNorthEast | NorthNorthWest | EastEastNorth | EastEastSouth | 
           SouthSouthEast | SouthSouthWest | WestWestNorth | WestWestSouth;
}

U64 computeKingAttacks(squareIndex square){
    U64 kingBB = 1ULL << square;
    U64 North = kingBB << 8;
    U64 South = kingBB >> 8;
    U64 East = (kingBB & notHFile) << 1;
    U64 West = (kingBB & notAFile) >> 1;
    U64 NorthEast = (kingBB & notHFile) << 9;
    U64 NorthWest = (kingBB & notAFile) << 7;
    U64 SouthEast = (kingBB & notHFile) >> 7;
    U64 SouthWest = (kingBB & notAFile) >> 9;
    return North     | South     | East      | West | 
           NorthEast | NorthWest | SouthEast | SouthWest;
}

U64 computePawnAttacks(squareIndex square, Color color){
    U64 pawnBB = 1ULL << square;
    U64 attacks = 0ULL;
    if(color == WHITE){
        attacks |= (pawnBB & notHFile) << 9;
        attacks |= (pawnBB & notAFile) << 7;
    } else {
        attacks |= (pawnBB & notHFile) >> 7;
        attacks |= (pawnBB & notAFile) >> 9;
    }
    return attacks;
}


// init functions - fill the attack tables at startup
void initKnightAttacks(){
    for(int square = 0; square < 64; square ++){
        knightAttacks[square] = computeKnightAttacks((squareIndex) square);
    }
}

void initKingAttacks(){
    for(int square = 0; square < 64; square ++){
        kingAttacks[square] = computeKingAttacks((squareIndex) square);
    }
}

void initPawnAttacks(){
    for(int square = 0; square < 64; square ++){
        pawnAttacks[WHITE][square] = computePawnAttacks((squareIndex) square, WHITE);
        pawnAttacks[BLACK][square] = computePawnAttacks((squareIndex) square, BLACK);
    }
}


U64 opponentPawnAttackMap(const Board& board, Color side){
    Color opp = (side == WHITE) ? BLACK : WHITE;
    U64 oppPawnAttacksBB = 0ULL;
    U64 oppPawns = board.pieces[opp][PAWN];

    while(oppPawns){
        squareIndex pawnSquare = (squareIndex)popLSB(oppPawns);
        oppPawnAttacksBB |= computePawnAttacks(pawnSquare, opp);
    }
    return oppPawnAttacksBB;
}
