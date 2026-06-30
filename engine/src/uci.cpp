#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "evaluation.h"
#include "search.h"
#include "tt.h"
#include "nnue.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <atomic>

namespace uci {

namespace {

Board board;
constexpr int INF = 1'000'000;
std::atomic<int> searchId{0};

using clock = std::chrono::steady_clock;
clock::time_point searchStart;

long long elapsed() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - searchStart).count();
}

// ── piece helpers ────────────────────────────────────────────────────────────

char promoLetter(PieceType pt) {
    switch (pt) {
        case QUEEN:  return 'q';
        case ROOK:   return 'r';
        case BISHOP: return 'b';
        case KNIGHT: return 'n';
        default:     return '\0';
    }
}

PieceType promoFromLetter(char c) {
    switch (c) {
        case 'q': return QUEEN;
        case 'r': return ROOK;
        case 'b': return BISHOP;
        case 'n': return KNIGHT;
        default:  return NONE;
    }
}

} // anonymous namespace

// ── square / move strings ────────────────────────────────────────────────────

int squareFromString(const std::string& s) {
    if (s.size() < 2) return -1;
    int file = s[0] - 'a';
    int rank = s[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return -1;
    return rank * 8 + file;
}

std::string squareToString(int sq) {
    if (sq < 0 || sq > 63) return "-";
    char file = 'a' + (sq % 8);
    char rank = '1' + (sq / 8);
    return std::string{file, rank};
}

std::string moveToString(const Move& move) {
    std::string s = squareToString(move.StartSquare) + squareToString(move.TargetSquare);
    char p = promoLetter(move.promoted);
    if (p) s += p;
    return s;
}

bool parseMove(Board& b, const std::string& uciMoveStr, Move& outMove) {
    if (uciMoveStr.size() < 4) return false;

    int from = squareFromString(uciMoveStr.substr(0, 2));
    int to   = squareFromString(uciMoveStr.substr(2, 2));
    if (from < 0 || to < 0) return false;

    PieceType promo = NONE;
    if (uciMoveStr.size() >= 5) promo = promoFromLetter(uciMoveStr[4]);

    MoveList legal = generateLegalMoves(b);
    for (int i = 0; i < legal.count; i++) {
        Move& m = legal.moves[i];
        if (m.StartSquare == from && m.TargetSquare == to && m.promoted == promo) {
            outMove = m;
            return true;
        }
    }
    return false;
}

// ── search ───────────────────────────────────────────────────────────────────

Move findBestMove(Board& b, int maxDepth) {
    searchStart = clock::now();
    stopSearch  = false;
    clearSearchTables();
    nodeCount = 0;

    Move best{};
    MoveList rootMoves = generateLegalMoves(b);
    if (rootMoves.count == 0) { best.StartSquare = -1; return best; }
    best = rootMoves.moves[0];

    initAccumulator(b, accStack[0]);
    int prevScore = 0;

    for (int depth = 1; depth <= maxDepth; depth++) {
        if (stopSearch.load()) break;

        int score;

        if (depth <= 5) {
            // full window for shallow depths — aspiration unreliable here
            score = alphaBeta(b, -INF, INF, depth, 0);
        } else {
            int delta = 40;
            int alpha = prevScore - delta;
            int beta  = prevScore + delta;

            while (true) {
                score = alphaBeta(b, alpha, beta, depth, 0);
                if (stopSearch.load()) break;

                if (score <= alpha) {
                    alpha -= delta;
                    delta *= 2;
                } else if (score >= beta) {
                    beta += delta;
                    delta *= 2;
                } else {
                    break;
                }
            }
        }

        if (stopSearch.load()) break; // discard partial result

        prevScore = score;

        for (int i = 1; i < rootMoves.count; i++) {
            if (rootMoves.moves[i] == best) {
                std::swap(rootMoves.moves[0], rootMoves.moves[i]);
                break;
            }
        }

        Move ttMove{};
        int dummy;
        if (TT.probeHash(b.hash, depth, dummy, -INF, INF, ttMove))
            best = ttMove;

        long long ms  = elapsed();
        long long nps = ms > 0 ? (nodeCount * 1000 / ms) : nodeCount;

        std::cout << "info depth " << depth
                  << " score cp "  << score
                  << " nodes "     << nodeCount
                  << " nps "       << nps
                  << " time "      << ms
                  << " pv "        << moveToString(best)
                  << std::endl;
    }

    return best;
}

// ── UCI command handlers ─────────────────────────────────────────────────────

namespace {

void handleUCI() {
    std::cout << "id name PaperRex\n";
    std::cout << "id author NotTofuu\n";
    std::cout << "uciok" << std::endl;
}

void handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void handlePosition(std::istringstream& iss) {
    clearSearchTables(); // killers/history are garbage from previous position
    nodeCount = 0;

    std::string token;
    iss >> token;

    if (token == "startpos") {
        loadStartPosition(board);
        iss >> token;
    } else if (token == "fen") {
        std::string fen, part;
        int fieldsCollected = 0;
        while (iss >> part) {
            if (part == "moves") { token = part; break; }
            if (fieldsCollected++) fen += " ";
            fen += part;
            token.clear();
        }
        loadPosition(board, fen);
    }

    board.historyIdx = 0; // reset AFTER loading position, BEFORE replaying moves

    if (token == "moves") {
        std::string moveStr;
        while (iss >> moveStr) {
            Move m;
            if (parseMove(board, moveStr, m)) {
                makeMove(board, m);
            } else {
                std::cerr << "info string illegal move: " << moveStr << std::endl;
                break;
            }
        }
    }
}

void handleGo(std::istringstream& iss) {
    std::string token;
    int depth          = -1;
    long long movetime = -1;
    long long wtime    = -1, btime = -1, winc = 0, binc = 0;
    int movestogo      = -1;

    while (iss >> token) {
        if      (token == "depth")     iss >> depth;
        else if (token == "movetime")  iss >> movetime;
        else if (token == "wtime")     iss >> wtime;
        else if (token == "btime")     iss >> btime;
        else if (token == "winc")      iss >> winc;
        else if (token == "binc")      iss >> binc;
        else if (token == "movestogo") iss >> movestogo;
        // "infinite" → allocatedMs stays -1, no timer
    }

    long long allocatedMs = -1;

    if (movetime > 0) {
        allocatedMs = movetime;
    } else if (wtime >= 0 || btime >= 0) {
        long long myTime = (board.sideToMove == WHITE) ? wtime : btime;
        long long myInc  = (board.sideToMove == WHITE) ? winc  : binc;
        if (myTime < 0) myTime = 0;

        int movesLeft = (movestogo > 0) ? movestogo : 40;
        allocatedMs = (myTime / movesLeft) + (myInc / 2);
        allocatedMs = std::min(allocatedMs, myTime / 2); // never burn more than half
        allocatedMs = std::max(allocatedMs, 10LL);
    }

    // bump search generation so stale timer threads don't fire into next search
    stopSearch    = false;
    int currentId = ++searchId;

    if (allocatedMs > 0) {
        std::thread([allocatedMs, currentId]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(allocatedMs));
            if (searchId.load() == currentId)
                stopSearch = true;
        }).detach();
    }

    Move best = findBestMove(board, depth < 0 ? 64 : depth);
    std::cout << "bestmove " << moveToString(best) << std::endl;
}

void handleNewGame() {
    board.historyIdx = 0;
    TT.clear();
    clearSearchTables();
    nodeCount = 0;
    ++searchId; // invalidate any lingering timer thread
}

} // anonymous namespace

// ── main loop ────────────────────────────────────────────────────────────────

void loop() {
    loadStartPosition(board);
    handleNewGame();

    if (!loadNetwork("D:/shinchess/model.bin")) {
        std::cerr << "Failed to load NNUE network" << std::endl;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if      (cmd == "uci")        handleUCI();
        else if (cmd == "isready")    handleIsReady();
        else if (cmd == "ucinewgame") handleNewGame();
        else if (cmd == "position")   handlePosition(iss);
        else if (cmd == "go")         handleGo(iss);
        else if (cmd == "quit")       break;
    }
}

} // namespace uci
