#include "chess_types.h"
#include "module3_analyst.h"
#include "module4_coach.h"
#include "module5_personalizer.h"
#include "engine_mapping.h"
#include "stockfish_bridge.h"
#include <iostream>
#include <cstdlib>
#include <sstream>

// ---------------------------------------------------------------------
// chess_bridge.exe has two modes:
//
//   chess_bridge.exe <time_spent_seconds> <move1> <move2> ... <moveN>
//       FAST PATH - used after every single move during live play.
//       Analyzes ONLY the last move (Module 3 + Module 4), gets the
//       engine's reply move, and prints ONE JSON object to stdout.
//       This is the only mode app.py's /analyze endpoint needs.
//
//   chess_bridge.exe --endgame <p_old_csv> <counts_csv> <move1> ... <moveN>
//       SLOW PATH - meant to run ONCE when a game ends. Replays the
//       full game (Module 3 aggregate stats), feeds Module 5
//       (personalization), and prints the updated profile + homework
//       so the CALLER (app.py, which owns the DB) can persist it and
//       load it back in next time.
//
//       p_old_csv:  10 comma-separated floats (the player's saved
//                    weakness profile) or the literal string "none"
//                    for a brand-new player (zero vector is used).
//       counts_csv: 10 comma-separated ints (consecutive-mistake
//                    counters) or "none" for a brand-new player.
//
// All diagnostic/progress logging goes to stderr. ONLY the JSON result
// goes to stdout, so app.py can safely do raw.find('{') / raw.rfind('}')
// without picking up debug text.
// ---------------------------------------------------------------------

std::vector<float> parseFloatCsv(const std::string& csv, int expectedSize) {
    std::vector<float> out;
    if (csv == "none" || csv.empty()) {
        out.assign(expectedSize, 0.0f);
        return out;
    }
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        out.push_back(std::stof(token));
    }
    while ((int)out.size() < expectedSize) out.push_back(0.0f);
    return out;
}

std::vector<int> parseIntCsv(const std::string& csv, int expectedSize) {
    std::vector<int> out;
    if (csv == "none" || csv.empty()) {
        out.assign(expectedSize, 0);
        return out;
    }
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        out.push_back(std::stoi(token));
    }
    while ((int)out.size() < expectedSize) out.push_back(0);
    return out;
}

void runFastPath(double timeSpentSeconds, const std::vector<std::string>& gameMoves) {
    if (gameMoves.empty()) {
        std::cerr << "ERROR: no moves supplied.\n";
        std::cout << "{\"success\": false, \"error\": \"no_moves\"}\n";
        return;
    }

    std::cerr << "Starting Stockfish...\n";
    StockfishBridge stockfish("stockfish.exe", 12);
    if (!stockfish.isRunning()) {
        std::cerr << "ERROR: Could not start Stockfish. Check the path.\n";
        std::cout << "{\"success\": false, \"error\": \"engine_start_failed\"}\n";
        return;
    }

    // Analyze ONLY the move that was just played - O(1) engine queries,
    // regardless of how many moves have been played so far in the game.
    RawMoveRecord raw = stockfish.analyzeLastMove(gameMoves, timeSpentSeconds);

    GameAnalyst analyst;
    ClassifiedMove cm = analyst.classifyMove(raw);

    // Ask the engine what it wants to play next - this becomes the bot's
    // reply move that the frontend animates onto the board.
    std::string engineMove = stockfish.getBestMove(gameMoves);

    std::cerr << "Move " << raw.move_number << " (" << raw.san << ") classified, "
              << "cp_loss=" << cm.centipawn_loss << ". Engine replies: " << engineMove << "\n";

    std::cout << buildMoveAnalysisJson(cm, engineMove) << "\n";
}

void runEndgamePath(const std::vector<std::string>& gameMoves,
                     const std::vector<float>& P_old,
                     std::vector<int>& counts) {
    if (gameMoves.empty()) {
        std::cout << "{\"success\": false, \"error\": \"no_moves\"}\n";
        return;
    }

    std::cerr << "Starting Stockfish for end-of-game analysis...\n";
    StockfishBridge stockfish("stockfish.exe", 12);
    if (!stockfish.isRunning()) {
        std::cout << "{\"success\": false, \"error\": \"engine_start_failed\"}\n";
        return;
    }

    std::vector<RawMoveRecord> rawMoves = stockfish.analyzeGame(gameMoves);
    GameAnalyst analyst;
    GameReport report = analyst.analyzeGame(rawMoves);

    // P_old / counts now come in from the caller (app.py), which loads them
    // from the database before calling this binary, and will save
    // update.new_profile / update.homework_motifs back to the database
    // after reading this output. This binary stays stateless.
    PersonalizationEngine personalizer;
    PersonalizationResult update = personalizer.runUpdate(P_old, report.motif_vector, counts);
    EngineConfig config = buildEngineConfig(update.sliders);

    std::cerr << "Endgame report: acpl=" << report.acpl
              << " accuracy=" << report.accuracy_pct << "\n";

    // Combine the game report with the personalization result into one
    // JSON payload for FastAPI to persist.
    std::string j = report.toJson();
    j.pop_back(); // drop trailing '}'
    j += ",\n  \"sliders\": [";
    for (int i = 0; i < (int)update.sliders.size(); i++) {
        j += std::to_string(update.sliders[i]);
        if (i < (int)update.sliders.size() - 1) j += ", ";
    }
    j += "],\n  \"engine_config\": {";
    j += "\"search_depth\": " + std::to_string(config.search_depth) + ", ";
    j += "\"skill_level\": " + std::to_string(config.skill_level) + ", ";
    j += "\"uci_elo\": " + std::to_string(config.uci_elo) + ", ";
    j += "\"contempt\": " + std::to_string(config.contempt);
    j += "},\n";

    // NEW: expose the updated profile + counts so app.py can persist them,
    // and the homework motif ids (as names) so app.py can save/display them.
    j += "  \"new_profile\": [";
    for (int i = 0; i < (int)update.new_profile.size(); i++) {
        j += std::to_string(update.new_profile[i]);
        if (i < (int)update.new_profile.size() - 1) j += ", ";
    }
    j += "],\n";

    j += "  \"updated_counts\": [";
    for (int i = 0; i < (int)counts.size(); i++) {
        j += std::to_string(counts[i]);
        if (i < (int)counts.size() - 1) j += ", ";
    }
    j += "],\n";

    j += "  \"homework_motifs\": [";
    for (int i = 0; i < (int)update.homework_motifs.size(); i++) {
        j += "\"" + motifToString(update.homework_motifs[i]) + "\"";
        if (i < (int)update.homework_motifs.size() - 1) j += ", ";
    }
    j += "]\n}";

    std::cout << j << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n";
        std::cerr << "  chess_bridge.exe <time_spent_seconds> <move1> ... <moveN>\n";
        std::cerr << "  chess_bridge.exe --endgame <p_old_csv|none> <counts_csv|none> <move1> ... <moveN>\n";
        std::cout << "{\"success\": false, \"error\": \"bad_usage\"}\n";
        return 1;
    }

    std::string firstArg = argv[1];

    if (firstArg == "--endgame") {
        if (argc < 4) {
            std::cerr << "ERROR: --endgame requires p_old_csv and counts_csv arguments.\n";
            std::cout << "{\"success\": false, \"error\": \"bad_usage\"}\n";
            return 1;
        }
        std::vector<float> P_old = parseFloatCsv(argv[2], NUM_MOTIFS);
        std::vector<int> counts = parseIntCsv(argv[3], NUM_MOTIFS);

        std::vector<std::string> gameMoves;
        for (int i = 4; i < argc; i++) gameMoves.push_back(std::string(argv[i]));
        runEndgamePath(gameMoves, P_old, counts);
        return 0;
    }

    double timeSpentSeconds = 0.0;
    try {
        timeSpentSeconds = std::stod(firstArg);
    } catch (...) {
        std::cerr << "WARNING: could not parse time_spent_seconds from '" << firstArg
                  << "', defaulting to 0.\n";
    }

    std::vector<std::string> gameMoves;
    for (int i = 2; i < argc; i++) gameMoves.push_back(std::string(argv[i]));

    runFastPath(timeSpentSeconds, gameMoves);
    return 0;
}
