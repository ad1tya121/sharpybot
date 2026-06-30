#pragma once
#include "board.h"
#include "movegen.h"
#include <string>

namespace uci {
    void loop();

    int squareFromString(const std::string& s);
    std::string squareToString(int sq);
    std::string moveToString(const Move& move);
    bool parseMove(Board& b, const std::string& uciMoveStr, Move& outMove);
    Move findBestMove(Board& b, int maxDepth);
}
