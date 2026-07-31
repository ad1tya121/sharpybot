#include "board.h"
#include "attacks.h"
#include "magic.h"
#include "movegen.h"
#include "search.h"
#include "evaluation.h"
#include "tt.h"
#include "zobrist.h"
#include "uci.h"
#include "perft.h"
#include "nnue.h"
#include <iostream>


int main() {
    initMagicTables();
    initPawnAttacks();
    initKnightAttacks();
    initKingAttacks();
    initZobrist();
    initLMR();
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    uci::loop();
    // runPerftTest(nullptr, 5, 4865609);
    // runPerftTest("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 6, 8031647685);
    // runPerftTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 1, 46);
    // runPerftTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 2, 2079);
    // runPerftTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 3, 89890);
    // runPerftTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 4, 3894594);
    // runPerftTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 5, 164075551);
    // runPerftTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 6, 6923051137);

    return 0;
}