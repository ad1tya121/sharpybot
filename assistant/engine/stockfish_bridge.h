#ifndef STOCKFISH_BRIDGE_H
#define STOCKFISH_BRIDGE_H

// HOW THIS FILE WORKS:
// Launches Stockfish as a child process. Communicates via stdin/stdout
// pipes using the UCI protocol (plain text lines).
//
// On Windows: uses CreateProcess + Windows pipes (HANDLE-based)
// On Linux:   uses fork/exec + POSIX pipes (int fd-based)
//
// The #ifdef _WIN32 blocks select the right implementation automatically.
// You don't need to change anything - just compile normally.
//
// TO SWITCH FROM STOCKFISH TO YOUR OWN ENGINE LATER:
// Change the stockfishPath argument in the constructor. Everything else
// stays the same because your engine will also speak UCI.

#include "chess_types.h"
#include "engine_mapping.h"
#include <vector>
#include <string>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

class StockfishBridge {
private:

#ifdef _WIN32
    HANDLE hChildStdinWrite;
    HANDLE hChildStdoutRead;
    PROCESS_INFORMATION procInfo;
#else
    int pipe_to_sf[2];
    int pipe_from_sf[2];
    pid_t sf_pid;
#endif

    bool running;
    int analysis_depth;

    // ---------------------------------------------------------------
    // Low-level pipe I/O
    // ---------------------------------------------------------------

    void sendCommand(const std::string& cmd) {
        std::string line = cmd + "\n";
#ifdef _WIN32
        DWORD written;
        WriteFile(hChildStdinWrite, line.c_str(), (DWORD)line.size(), &written, NULL);
#else
        ssize_t w = write(pipe_to_sf[1], line.c_str(), line.size());
        (void)w;
#endif
    }

    // Reads lines from Stockfish one by one until a line containing
    // `marker` is found. Returns all lines received.
    std::string readUntil(const std::string& marker) {
        std::string result;
        std::string line;
        char ch;

        while (true) {
            line = "";
#ifdef _WIN32
            DWORD bytesRead;
            while (ReadFile(hChildStdoutRead, &ch, 1, &bytesRead, NULL) && bytesRead > 0) {
                if (ch == '\n') break;
                if (ch != '\r') line += ch;
            }
#else
            while (read(pipe_from_sf[0], &ch, 1) == 1) {
                if (ch == '\n') break;
                line += ch;
            }
#endif
            result += line + "\n";
            if (line.find(marker) != std::string::npos) break;
        }
        return result;
    }

    // ---------------------------------------------------------------
    // Parsing Stockfish's "info" output lines
    // ---------------------------------------------------------------

    // Extracts centipawn score from a line like:
    // "info depth 12 ... score cp 47 ..."
    // For mate scores, returns +/-30000 (a large value outside normal range)
    int parseScore(const std::string& infoLine) {
        size_t cpPos = infoLine.find("score cp ");
        if (cpPos != std::string::npos) {
            std::string rest = infoLine.substr(cpPos + 9);
            size_t end = rest.find(' ');
            return std::stoi(rest.substr(0, end));
        }
        size_t matePos = infoLine.find("score mate ");
        if (matePos != std::string::npos) {
            std::string rest = infoLine.substr(matePos + 11);
            int mateIn = std::stoi(rest.substr(0, rest.find(' ')));
            return (mateIn > 0) ? 30000 : -30000;
        }
        return 0;
    }

    // Extracts the principal variation (best line) from an info line.
    // e.g. "... pv e2e4 e7e5 g1f3" -> "e2e4 e7e5 g1f3"
    std::string parsePV(const std::string& infoLine) {
        size_t pvPos = infoLine.find(" pv ");
        if (pvPos == std::string::npos) return "";
        std::string pv = infoLine.substr(pvPos + 4);
        size_t nl = pv.find('\n');
        if (nl != std::string::npos) pv = pv.substr(0, nl);
        // Remove trailing \r on Windows
        if (!pv.empty() && pv.back() == '\r') pv.pop_back();
        return pv;
    }

    // From all the "info depth X ..." lines Stockfish printed,
    // returns the LAST one (highest depth reached = best result).
    std::string getLastInfoLine(const std::string& allOutput) {
        std::string last;
        std::istringstream stream(allOutput);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("info depth") != std::string::npos &&
                line.find("score") != std::string::npos &&
                line.find(" pv ") != std::string::npos) {
                last = line;
            }
        }
        return last;
    }

    // ---------------------------------------------------------------
    // Core evaluation function
    // ---------------------------------------------------------------

    struct EvalResult {
        int score_cp;        // centipawn score from current side's perspective
        std::string best_pv; // e.g. "e2e4 e7e5 g1f3"
    };

    // Sends a position to Stockfish and gets back the eval + best line.
    // movesSoFar: list of UCI moves already played, e.g. {"e2e4","e7e5"}
    EvalResult evaluate(const std::vector<std::string>& movesSoFar) {
        std::string posCmd = "position startpos";
        if (!movesSoFar.empty()) {
            posCmd += " moves";
            for (const std::string& m : movesSoFar) posCmd += " " + m;
        }
        sendCommand(posCmd);
        sendCommand("go depth " + std::to_string(analysis_depth));

        std::string output = readUntil("bestmove");
        std::string lastInfo = getLastInfoLine(output);

        EvalResult result;
        result.score_cp = parseScore(lastInfo);
        result.best_pv  = parsePV(lastInfo);
        return result;
    }

    // ---------------------------------------------------------------
    // Motif detection heuristics
    // ---------------------------------------------------------------
    // These are APPROXIMATIONS using only eval numbers and move context.
    // They work reasonably for demo/testing but will miss edge cases.
    //
    // When your own engine (Module 1/2) is ready, REPLACE the body of
    // each detectX() function with a call to your engine's actual board
    // analysis (attack maps, pin detection, etc.) and use those results
    // to set the flags instead. The function signatures stay the same.
    // ---------------------------------------------------------------

    // A piece is likely hanging if the eval is already very bad from the
    // current player's perspective AND the cp loss is massive (suggesting
    // a free capture was available). We use a high threshold (250) to
    // avoid false positives - a genuine hanging piece usually causes a
    // very large swing.
    bool detectHangingPiece(int cpLoss, int evalBefore) {
        // Player was in a reasonable position but dropped >250cp = probably hung something
        return (cpLoss > 250 && evalBefore > -100);
    }

    // A fork typically causes a large-ish loss from a GOOD position, and
    // the engine's best line tends to be short (a forcing fork sequence
    // is usually 2-3 moves, so PV has few tokens).
    // We check: was the player winning/equal? Did they lose a lot?
    // Did the engine want a short forcing reply?
    bool detectMissedFork(int cpLoss, int evalBefore, const std::string& bestPv) {
        if (cpLoss < 80)    return false;  // too small to be a missed fork
        if (evalBefore < 0) return false;  // already losing, probably not a fork opportunity

        // Count how many moves are in the best PV
        // A fork follow-up is usually 2-5 moves. Long PVs (10+ moves)
        // are positional play, not a fork.
        int pvMoveCount = 0;
        std::istringstream ss(bestPv);
        std::string token;
        while (ss >> token) pvMoveCount++;

        return (pvMoveCount >= 2 && pvMoveCount <= 5);
    }

    // King safety collapse: significant eval drop (>150cp) from a position
    // that was NOT already lost. A king safety problem is characterized by
    // a sudden large swing rather than a slow deterioration.
    bool detectKingSafetyCollapse(int cpLoss, int evalBefore) {
        return (cpLoss > 150 && evalBefore > -80);
    }

    // Back rank weakness: bad move in the ENDGAME (phase 2) with a large
    // eval drop. Back rank issues are almost exclusively an endgame problem
    // (in the middlegame you have enough pieces to defend).
    // We deliberately require phase == 2 to avoid false positives.
    bool detectBackRankWeakness(int cpLoss, int phase) {
        return (cpLoss > 120 && phase == 2);
    }

    // Missed pin/skewer: medium-sized loss (not catastrophic like a blunder)
    // from an equal position, where we haven't already attributed the loss
    // to a fork, king safety issue, or hanging piece. Pin/skewer
    // opportunities usually exist in roughly equal positions.
    bool detectMissedPinSkewer(int cpLoss, int evalBefore,
                                bool isFork, bool isKingSafety, bool isHanging) {
        if (isFork || isKingSafety || isHanging) return false; // already explained
        return (cpLoss >= 60 && cpLoss <= 200 && evalBefore > -50 && evalBefore < 200);
    }

public:
    // ---------------------------------------------------------------
    // Constructor: launches Stockfish
    // ---------------------------------------------------------------
    // stockfishPath:
    //   Windows: "C:/path/to/stockfish.exe"  or just "stockfish.exe"
    //            if it's on your PATH
    //   Linux:   "/usr/games/stockfish"
    StockfishBridge(const std::string& stockfishPath = "./engine/engine.exe", int depth = 12) {
        analysis_depth = depth;
        running = false;

#ifdef _WIN32
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hChildStdinRead, hChildStdoutWrite;
        CreatePipe(&hChildStdinRead,  &hChildStdinWrite,  &sa, 0);
        CreatePipe(&hChildStdoutRead, &hChildStdoutWrite, &sa, 0);

        // Child process should NOT inherit our ends of the pipes
        SetHandleInformation(hChildStdinWrite,  HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hChildStdoutRead,  HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb          = sizeof(si);
        si.hStdInput   = hChildStdinRead;
        si.hStdOutput  = hChildStdoutWrite;
        si.hStdError   = hChildStdoutWrite;
        si.dwFlags    |= STARTF_USESTDHANDLES;

        ZeroMemory(&procInfo, sizeof(procInfo));

        std::string cmd = stockfishPath;
        BOOL ok = CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW, NULL, NULL, &si, &procInfo);

        CloseHandle(hChildStdinRead);
        CloseHandle(hChildStdoutWrite);

        if (!ok) {
            printf("ERROR: Could not start Stockfish at: %s\n", stockfishPath.c_str());
            printf("Make sure the path is correct and stockfish.exe exists there.\n");
            return;
        }
#else
        if (pipe(pipe_to_sf) != 0 || pipe(pipe_from_sf) != 0) return;

        sf_pid = fork();
        if (sf_pid == 0) {
            dup2(pipe_to_sf[0],  STDIN_FILENO);
            dup2(pipe_from_sf[1], STDOUT_FILENO);
            close(pipe_to_sf[1]);
            close(pipe_from_sf[0]);
            execl(stockfishPath.c_str(), stockfishPath.c_str(), nullptr);
            exit(1);
        }
        close(pipe_to_sf[0]);
        close(pipe_from_sf[1]);
#endif

        // Handshake: wait for "uciok" then "readyok"
        sendCommand("uci");
        readUntil("uciok");
        sendCommand("isready");
        readUntil("readyok");

        running = true;
    }

    ~StockfishBridge() {
        if (!running) return;
        sendCommand("quit");
#ifdef _WIN32
        WaitForSingleObject(procInfo.hProcess, 3000);
        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
        CloseHandle(hChildStdinWrite);
        CloseHandle(hChildStdoutRead);
#else
        waitpid(sf_pid, nullptr, 0);
        close(pipe_to_sf[1]);
        close(pipe_from_sf[0]);
#endif
    }

    bool isRunning() { return running; }

    // Applies the personalization settings to Stockfish before the
    // NEXT game. Call this after runUpdate() gives you the EngineConfig.
    void applyPersonalization(const EngineConfig& config) {
        if (!running) return;
        sendCommand("setoption name Skill Level value " + std::to_string(config.skill_level));
        sendCommand("setoption name UCI_LimitStrength value true");
        sendCommand("setoption name UCI_Elo value " + std::to_string(config.uci_elo));
        sendCommand("setoption name Contempt value " + std::to_string(config.contempt));
        sendCommand("isready");
        readUntil("readyok");
    }

    // ---------------------------------------------------------------
    // analyzeGame() - the main function your pipeline calls
    // ---------------------------------------------------------------
    // Takes the list of UCI moves played (e.g. {"e2e4","e7e5","g1f3"})
    // and evaluates each one using Stockfish to fill in RawMoveRecord.
    // Returns one RawMoveRecord per move.
    std::vector<RawMoveRecord> analyzeGame(const std::vector<std::string>& uciMoves) {
        std::vector<RawMoveRecord> records;
        if (!running || uciMoves.empty()) return records;

        std::vector<std::string> playedSoFar;

        for (int i = 0; i < (int)uciMoves.size(); i++) {
            bool whiteToMove  = (i % 2 == 0);
            int fullMoveNum   = (i / 2) + 1;

            // Get eval BEFORE this move
            EvalResult before = evaluate(playedSoFar);

            // Stockfish always reports from the perspective of the side
            // to move. We store as "from active player's perspective" so
            // both White and Black evals are comparable.
            int evalBefore = before.score_cp;

            // Add this move to the history and get eval AFTER
            playedSoFar.push_back(uciMoves[i]);
            EvalResult after = evaluate(playedSoFar);

            // After the move, it's the OTHER side's turn. Negate because
            // we want "how good is this for the player who JUST moved".
            int evalAfter = -after.score_cp;

            int cpLoss = evalBefore - evalAfter;

            // Phase heuristic: opening=first 10 moves, endgame=after 30,
            // middlegame=everything between.
            int phase = 1;
            if (fullMoveNum <= 10) phase = 0;
            else if (fullMoveNum >= 30) phase = 2;

            // Run all motif detections
            bool isHanging    = detectHangingPiece(cpLoss, evalBefore);
            bool isFork       = detectMissedFork(cpLoss, evalBefore, before.best_pv);
            bool isKingSafety = detectKingSafetyCollapse(cpLoss, evalBefore);
            bool isBackRank   = detectBackRankWeakness(cpLoss, phase);
            bool isPinSkewer  = detectMissedPinSkewer(cpLoss, evalBefore, isFork, isKingSafety, isHanging);

            RawMoveRecord rec;
            rec.move_number        = fullMoveNum;
            rec.is_white_to_move   = whiteToMove;
            rec.san                = uciMoves[i];
            rec.eval_before_cp     = evalBefore;
            rec.eval_after_cp      = evalAfter;
            rec.engine_best_pv     = before.best_pv;
            rec.time_spent_seconds = 0.0;
            rec.phase              = phase;
            rec.left_piece_hanging   = isHanging;
            rec.missed_fork_available = isFork;
            rec.king_safety_collapsed = isKingSafety;
            rec.back_rank_weak       = isBackRank;
            rec.missed_pin_or_skewer  = isPinSkewer;

            records.push_back(rec);
        }

        return records;
    }

    // ---------------------------------------------------------------
    // analyzeLastMove() - the FAST path used for live per-move coaching
    // ---------------------------------------------------------------
    // Unlike analyzeGame(), this does NOT replay the whole game. It only
    // evaluates the position right before the last move and right after
    // it - 2 engine queries total, regardless of how long the game is.
    // This is what keeps live analysis fast as the game goes on.
    // timeSpentSeconds: how long the player spent on THIS move (used for
    // the time-pressure-blunder motif). Pass 0 if unknown/untracked.
    RawMoveRecord analyzeLastMove(const std::vector<std::string>& allMoves, double timeSpentSeconds = 0.0) {
        RawMoveRecord rec{};
        if (!running || allMoves.empty()) return rec;

        std::vector<std::string> beforeMoves(allMoves.begin(), allMoves.end() - 1);
        int moveIndex   = (int)allMoves.size() - 1;
        bool whiteToMove = (moveIndex % 2 == 0);
        int fullMoveNum  = (moveIndex / 2) + 1;

        EvalResult before = evaluate(beforeMoves);
        int evalBefore = before.score_cp;

        EvalResult after = evaluate(allMoves);
        int evalAfter = -after.score_cp;

        int cpLoss = evalBefore - evalAfter;

        int phase = 1;
        if (fullMoveNum <= 10) phase = 0;
        else if (fullMoveNum >= 30) phase = 2;

        bool isHanging    = detectHangingPiece(cpLoss, evalBefore);
        bool isFork       = detectMissedFork(cpLoss, evalBefore, before.best_pv);
        bool isKingSafety = detectKingSafetyCollapse(cpLoss, evalBefore);
        bool isBackRank   = detectBackRankWeakness(cpLoss, phase);
        bool isPinSkewer  = detectMissedPinSkewer(cpLoss, evalBefore, isFork, isKingSafety, isHanging);

        rec.move_number         = fullMoveNum;
        rec.is_white_to_move    = whiteToMove;
        rec.san                 = allMoves[moveIndex];
        rec.eval_before_cp      = evalBefore;
        rec.eval_after_cp       = evalAfter;
        rec.engine_best_pv      = before.best_pv;
        rec.time_spent_seconds  = timeSpentSeconds;
        rec.phase               = phase;
        rec.left_piece_hanging    = isHanging;
        rec.missed_fork_available = isFork;
        rec.king_safety_collapsed = isKingSafety;
        rec.back_rank_weak        = isBackRank;
        rec.missed_pin_or_skewer  = isPinSkewer;
        return rec;
    }

    // Returns Stockfish's best move for the current position as a UCI
    // string, e.g. "e2e4". Used to make the bot's reply move.
    std::string getBestMove(const std::vector<std::string>& playedMoves) {
        if (!running) return "";
        EvalResult result = evaluate(playedMoves);
        if (result.best_pv.empty()) return "";
        size_t sp = result.best_pv.find(' ');
        return (sp == std::string::npos) ? result.best_pv : result.best_pv.substr(0, sp);
    }
};

#endif
