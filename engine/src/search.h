#pragma once
#include "board.h"
#include <atomic>

const int mateScore = 100000;
const int infinity = 3000000;
extern long long nodeCount;

int alphaBeta(Board& board, int alpha, int beta, int depth, int ply);
int quiescence(Board& board, int alpha, int beta, int ply);
void orderMoves(Board& board, MoveList& moves, Move ttBestMove, int ply);
extern std::atomic<bool> stopSearch;

// killer moves[id][ply]
extern Move killer_moves[2][256];

// history moves [piece][square]
extern int history_moves[2][6][64];

void clearSearchTables();
