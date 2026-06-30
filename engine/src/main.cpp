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
#include <iostream>


int main() {
    initMagicTables();
    initPawnAttacks();
    initKnightAttacks();
    initKingAttacks();
    initZobrist();
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    uci::loop();
    return 0;
}
