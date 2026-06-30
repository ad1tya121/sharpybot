#pragma once
#include "types.h"
#include "movegen.h"
#include <string>

long long perft(Board& board, int depth);
void runPerftTest(const char* fen, int depth, long long expectedNodes);
