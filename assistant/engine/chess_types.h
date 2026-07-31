#ifndef CHESS_TYPES_H
#define CHESS_TYPES_H

#include <vector>
#include <string>
#include <cstdio>

// Escapes a string for safe embedding inside a JSON string literal.
// Every place in this codebase that builds JSON by hand MUST run
// dynamic strings (SAN, PV, LLM prompts, etc.) through this first -
// otherwise a stray quote or newline produces invalid JSON that
// Python's json.loads() will reject.
inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

// The 10 tactical weaknesses. Their INTEGER VALUES (0-9) are used as
// array indices throughout the code - never reorder these.
enum TacticalMotif {
    HANGING_PIECE = 0,
    MISSED_FORK = 1,
    KING_SAFETY_COLLAPSE = 2,
    BACK_RANK_WEAKNESS = 3,
    MISSED_PIN_SKEWER = 4,
    TIME_PRESSURE_BLUNDER = 5,
    DEFENSIVE_INACCURACY = 6,
    OFFENSIVE_MISS = 7,
    TACTICAL_BLINDNESS = 8,
    POSITIONAL_MISJUDGMENT = 9,
    NUM_MOTIFS = 10
};

// The 5 engine personality sliders. Integer values = array indices,
// same rule as above.
enum EngineSlider {
    DIFFICULTY_CAP = 0,
    AGGRESSION_BIAS = 1,
    TACTICAL_COMPLEXITY = 2,
    ENDGAME_EMPHASIS = 3,
    CONTEMPT_FACTOR = 4,
    NUM_SLIDERS = 5
};

enum MoveQuality {
    BRILLIANT = 0,
    BEST = 1,
    GOOD = 2,
    INACCURACY = 3,
    MISTAKE = 4,
    BLUNDER = 5
};

inline void describeQuality(MoveQuality quality, std::string& outLabel, std::string& outColor) {
    if      (quality == BRILLIANT)   { outLabel = "Brilliant";  outColor = "cyan";   }
    else if (quality == BEST)        { outLabel = "Best";       outColor = "green";  }
    else if (quality == GOOD)        { outLabel = "Good";       outColor = "green";  }
    else if (quality == INACCURACY)  { outLabel = "Inaccuracy"; outColor = "yellow"; }
    else if (quality == MISTAKE)     { outLabel = "Mistake";    outColor = "orange"; }
    else                             { outLabel = "Blunder";    outColor = "red";    }
}

inline std::string motifToString(int motif) {
    if (motif == HANGING_PIECE)          return "hanging_piece";
    if (motif == MISSED_FORK)            return "missed_fork";
    if (motif == KING_SAFETY_COLLAPSE)   return "king_safety_collapse";
    if (motif == BACK_RANK_WEAKNESS)     return "back_rank_weakness";
    if (motif == MISSED_PIN_SKEWER)      return "missed_pin_skewer";
    if (motif == TIME_PRESSURE_BLUNDER)  return "time_pressure_blunder";
    if (motif == DEFENSIVE_INACCURACY)   return "defensive_inaccuracy";
    if (motif == OFFENSIVE_MISS)         return "offensive_miss";
    if (motif == TACTICAL_BLINDNESS)     return "tactical_blindness";
    if (motif == POSITIONAL_MISJUDGMENT) return "positional_misjudgment";
    return "unknown";
}

// ONE move's raw data - filled in by StockfishBridge (or later, your own engine).
// phase: 0=opening, 1=middlegame, 2=endgame
// The 5 booleans: currently set by heuristics in StockfishBridge.
//   When your own engine is ready, replace those heuristics with real
//   bitboard-based detection and set these flags from there instead.
struct RawMoveRecord {
    int move_number;
    bool is_white_to_move;
    std::string san;
    int eval_before_cp;
    int eval_after_cp;
    std::string engine_best_pv;
    double time_spent_seconds;
    int phase;
    bool left_piece_hanging;
    bool missed_fork_available;
    bool king_safety_collapsed;
    bool back_rank_weak;
    bool missed_pin_or_skewer;
};

// RawMoveRecord + Module 3's judgments layered on top.
struct ClassifiedMove {
    RawMoveRecord raw;
    int centipawn_loss;
    MoveQuality quality;
    std::vector<int> triggered_motifs;
};

// Full game summary. motif_vector (size 10) is x_t - fed into Module 5.
// toJson() lets FastAPI consume this output directly.
struct GameReport {
    double accuracy_pct;
    double acpl;
    int blunders;
    int mistakes;
    int inaccuracies;
    std::vector<double> phase_acpl;
    std::vector<float> motif_vector;
    std::vector<ClassifiedMove> moves;

    // Serializes the whole report to a JSON string.
    // FastAPI reads this and forwards it to the frontend / Gemini.
    std::string toJson() const {
        std::string j = "{\n";
        j += "  \"accuracy_pct\": " + std::to_string(accuracy_pct) + ",\n";
        j += "  \"acpl\": " + std::to_string(acpl) + ",\n";
        j += "  \"blunders\": " + std::to_string(blunders) + ",\n";
        j += "  \"mistakes\": " + std::to_string(mistakes) + ",\n";
        j += "  \"inaccuracies\": " + std::to_string(inaccuracies) + ",\n";

        j += "  \"phase_acpl\": [";
        for (int i = 0; i < (int)phase_acpl.size(); i++) {
            j += std::to_string(phase_acpl[i]);
            if (i < (int)phase_acpl.size() - 1) j += ", ";
        }
        j += "],\n";

        j += "  \"motif_vector\": [";
        for (int i = 0; i < (int)motif_vector.size(); i++) {
            j += std::to_string(motif_vector[i]);
            if (i < (int)motif_vector.size() - 1) j += ", ";
        }
        j += "],\n";

        j += "  \"moves\": [\n";
        for (int i = 0; i < (int)moves.size(); i++) {
            const ClassifiedMove& cm = moves[i];
            std::string label, color;
            describeQuality(cm.quality, label, color);
            j += "    {";
            j += "\"move_number\": " + std::to_string(cm.raw.move_number) + ", ";
            j += "\"san\": \"" + jsonEscape(cm.raw.san) + "\", ";
            j += "\"cp_loss\": " + std::to_string(cm.centipawn_loss) + ", ";
            j += "\"quality\": \"" + label + "\", ";
            j += "\"color\": \"" + color + "\", ";
            j += "\"engine_best_pv\": \"" + jsonEscape(cm.raw.engine_best_pv) + "\", ";
            j += "\"motifs\": [";
            for (int m = 0; m < (int)cm.triggered_motifs.size(); m++) {
                j += "\"" + motifToString(cm.triggered_motifs[m]) + "\"";
                if (m < (int)cm.triggered_motifs.size() - 1) j += ", ";
            }
            j += "]}";
            if (i < (int)moves.size() - 1) j += ",";
            j += "\n";
        }
        j += "  ]\n}";
        return j;
    }
};

#endif
