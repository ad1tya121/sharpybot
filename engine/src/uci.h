#pragma once
#include "board.h"
#include "movegen.h"
#include <string>
#include <thread>
#include <chrono>

namespace uci {
    using clock = std::chrono::steady_clock; // Makes your code cleaner
    extern clock::time_point searchStart;
    extern clock::time_point searchEnd;
    
    // --- ADDED THIS LINE RIGHT HERE ---
    extern bool useTimer;
    void loop();

    int squareFromString(const std::string& s);
    std::string squareToString(int sq);
    std::string moveToString(const Move& move);
    bool parseMove(Board& b, const std::string& uciMoveStr, Move& outMove);
    Move findBestMove(Board& b, int maxDepth);
}
