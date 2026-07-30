#pragma once
#include "board.h"
#include "types.h"

U64 attackersTo(const Board& board, int sq, U64 occupied);
PieceType movingPieceAt(const Board& board, Color side, int sq);
bool seeGE(const Board& board, Move move, int threshold);