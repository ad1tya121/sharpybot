#pragma once
#include "board.h"
#include <cstdint>

typedef uint64_t U64;

struct ZobristData {
    U64 z_pieces[12][64];
    U64 z_castling[16];
    U64 z_enPassant[8];
    U64 z_side;
};

extern ZobristData zobrist;
void initZobrist();
inline void togglePieceHash(Board& board, Color side, int piece, int square) {
    if (piece != NONE) {
        int zIndex = (side * 6) + piece;
        board.hash ^= zobrist.z_pieces[zIndex][square];
    }
}
