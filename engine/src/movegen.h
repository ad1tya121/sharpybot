#pragma once
#include "board.h"

enum GenMode { all, onlyCaptures };
void makeMove(Board& board, Move& move, int ply = 0);
void unMakeMove(Board& board, Move& move);
MoveList generateLegalMoves(Board& board, GenMode mode = all);
bool isKingInCheck(Board& board, Color side);
bool isSquareAttackedBy(Board& board, int sq, Color attackerSide);
PieceType getCaptured(Board& board, Color opp, int sq);
bool isPromotionSquare(Color side, int sq);
bool hasNonPawnMaterial(Board& board, Color side);
NullMoveUndo makeNullMove(Board& board);
void unmakeNullMove(Board& board, const NullMoveUndo& undo);
