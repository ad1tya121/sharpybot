#pragma once
#include <vector>
#include "types.h"
#include "board.h"

U64 computeRookMask(squareIndex square);
U64 computeBishopMask(squareIndex square);
U64 computeRookAttacks(squareIndex square, U64 blockers);
U64 computeBishopAttacks(squareIndex square, U64 blockers);
std::vector<U64> initBlockers(U64 mask);

struct Smagic {
    U64* ptr;
    U64 mask;
    U64 magic;
    int shift;
};

extern Smagic mRookTable[64]; 
extern Smagic mBishopTable[64];

void initMagicTables();

inline U64 rookAttacks(int square, U64 boardOccupancy) {
    U64 blockers = boardOccupancy & mRookTable[square].mask;
    unsigned int index = (blockers * mRookTable[square].magic) >> mRookTable[square].shift;
    return mRookTable[square].ptr[index];
}

inline U64 bishopAttacks(int square, U64 boardOccupancy) {
    U64 blockers = boardOccupancy & mBishopTable[square].mask;
    unsigned int index = (blockers * mBishopTable[square].magic) >> mBishopTable[square].shift;
    return mBishopTable[square].ptr[index];
}
