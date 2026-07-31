#ifndef ENGINE_MAPPING_H
#define ENGINE_MAPPING_H

#include "chess_types.h"
#include <vector>
#include <string>
#include <cmath>

struct EngineConfig {
    int search_depth;
    int skill_level;
    int uci_elo;
    int contempt;
    bool prefer_tactics;
    bool prefer_endgame_play;
};

// Maps a value from -1..+1 onto any target range [minOut, maxOut].
// e.g. mapRange(0.5, 0, 20) = 15  (75% of the way from 0 to 20)
inline double mapRange(float sliderValue, double minOut, double maxOut) {
    double fraction = (sliderValue + 1.0) / 2.0;
    return fraction * (maxOut - minOut) + minOut;
}

inline EngineConfig buildEngineConfig(const std::vector<float>& sliders) {
    EngineConfig config;

    config.search_depth = (int)round(mapRange(sliders[DIFFICULTY_CAP], 4.0, 18.0));
    config.skill_level  = (int)round(mapRange(sliders[DIFFICULTY_CAP], 0.0, 20.0));

    // UCI_Elo range: 1320 (Skill 0) to 3190 (Skill 20), Stockfish's actual limits
    config.uci_elo = 1320 + (int)((config.skill_level / 20.0) * (3190 - 1320));

    config.contempt = (int)round(mapRange(sliders[AGGRESSION_BIAS], -50.0, 50.0));

    config.prefer_tactics     = (sliders[TACTICAL_COMPLEXITY] > 0.0f);
    config.prefer_endgame_play = (sliders[ENDGAME_EMPHASIS] > 0.0f);

    return config;
}

// Returns the UCI command strings to send to Stockfish.
inline std::vector<std::string> buildUciCommands(const EngineConfig& config) {
    std::vector<std::string> cmds;
    cmds.push_back("setoption name Skill Level value " + std::to_string(config.skill_level));
    cmds.push_back("setoption name UCI_LimitStrength value true");
    cmds.push_back("setoption name UCI_Elo value " + std::to_string(config.uci_elo));
    cmds.push_back("setoption name Contempt value " + std::to_string(config.contempt));
    cmds.push_back("go depth " + std::to_string(config.search_depth));
    return cmds;
}

#endif
