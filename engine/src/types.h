#pragma once
#include <cstdint>

typedef uint64_t U64;


// enums
enum squareIndex {
                    A1, B1, C1, D1, E1, F1, G1, H1,
                    A2, B2, C2, D2, E2, F2, G2, H2,
                    A3, B3, C3, D3, E3, F3, G3, H3,
                    A4, B4, C4, D4, E4, F4, G4, H4,
                    A5, B5, C5, D5, E5, F5, G5, H5,
                    A6, B6, C6, D6, E6, F6, G6, H6,
                    A7, B7, C7, D7, E7, F7, G7, H7,
                    A8, B8, C8, D8, E8, F8, G8, H8 };
enum Color {WHITE, BLACK};
enum PieceType {NONE = -1, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};
// enum NNUEPieceType {PAWN, KNIGHT, BISHOP, ROOK, QUEEN};

inline U64  getBit  (U64 bb, int sq)  { return bb & (1ULL << sq); }
inline void setBit  (U64& bb, int sq) { bb |= (1ULL << sq); }  // mark the bit on that square = 1
inline void clearBit(U64& bb, int sq) { bb &= ~(1ULL << sq); } // mark the bit on that square = 0 
inline int getLSB   (U64 bb)          { return __builtin_ctzll(bb); } // find the index of least significant bit
inline int popLSB   (U64& bb)         { int sq = getLSB(bb); bb &= bb - 1; return sq; }
inline int countBits(U64 bb)          { return __builtin_popcountll(bb); }

constexpr uint8_t WK_CASTLE = 0b0001; // white kingside
constexpr uint8_t WQ_CASTLE = 0b0010; // white queenside
constexpr uint8_t BK_CASTLE = 0b0100; // black kingside
constexpr uint8_t BQ_CASTLE = 0b1000; // black queenside
