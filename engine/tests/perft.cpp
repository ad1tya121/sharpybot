#include "board.h"
#include "movegen.h"
#include "magic.h"
#include "attacks.h"
#include "zobrist.h"
#include "perft.h"
#include <iostream>
#include <chrono>


long long perft(Board& board, int depth) {
    if (depth == 0) return 1;
    MoveList moves = generateLegalMoves(board);
    if (depth == 1) return moves.count;
    long long nodes = 0;
    for (int i = 0; i < moves.count; i++) {
        makeMove(board, moves.moves[i]);
        nodes += perft(board, depth - 1);
        unMakeMove(board, moves.moves[i]);
    }
    return nodes;
}

void runPerftTest(const char* fen, int depth, long long expected) {
    Board board;
    if (fen == nullptr) loadStartPosition(board);
    else loadPosition(board, fen);

    auto t0 = std::chrono::steady_clock::now();
    long long nodes = perft(board, depth);
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    std::cout << "depth " << depth << " nodes " << nodes
              << " expected " << expected
              << (nodes == expected ? "  [PASS]" : "  [FAIL]")
              << "  time " << ms << "ms\n";
}
