#pragma once
#include "board.h"

// inline const used because this will define once in header and 
// no other file which needs this wont make another copy of it

inline const U64 notAFile = ~0x0101010101010101ULL;
inline const U64 notHFile = ~0x8080808080808080ULL;
inline const U64 notBFile = ~0x0202020202020202ULL;
inline const U64 notGFile = ~0x4040404040404040ULL;
inline const U64 notABFile = notAFile & notBFile;
inline const U64 notGHFile = notGFile & notHFile;

// AttackTables 
extern U64 knightAttacks[64];
extern U64 kingAttacks[64];
extern U64 pawnAttacks[2][64];

// Init functions - call once when startup in main()
void initKnightAttacks();
void initKingAttacks();
void initPawnAttacks();

// Compute functions - called only by init , not for search
U64 computeKnightAttacks(squareIndex square);
U64 computeKingAttacks(squareIndex square);
U64 computePawnAttacks(squareIndex square, Color color);



U64 opponentPawnAttackMap(const Board& board, Color side);
