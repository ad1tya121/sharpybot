#ifndef MODULE4_COACH_H
#define MODULE4_COACH_H

// MODULE 4 - What this file does in YOUR architecture:
// C++ side: builds the XML snapshot string and the prompt text.
// FastAPI side: receives that XML/prompt, forwards it to Gemini API.
// This file does NOT make any HTTP/LLM calls - that's Python's job.

#include "chess_types.h"
#include <string>
#include <vector>

inline std::string extractFirstMove(const std::string& pv) {
    if (pv.empty()) return "None";
    size_t spacePos = pv.find(' ');
    if (spacePos == std::string::npos) return pv;
    return pv.substr(0, spacePos);
}

// True when the move the player actually played IS the engine's #1 choice
// in that position. In that case there is no "better alternative" to show -
// showing one anyway (as the old code did) is misleading and demoralizing.
// NOTE: move.raw.san currently holds the UCI string of the move played
// (set in StockfishBridge::analyzeLastMove/analyzeGame), so comparing it
// directly against extractFirstMove(engine_best_pv) - also UCI - is correct.
inline bool isTopEngineChoice(const ClassifiedMove& move) {
    std::string bestMove = extractFirstMove(move.raw.engine_best_pv);
    return bestMove != "None" && bestMove == move.raw.san;
}

// Builds the <engine_snapshot> XML block (proposal Section 6.1).
// This is what gets sent to FastAPI, which wraps it in a prompt and
// forwards it to Gemini.
inline std::string buildSnapshotXml(const ClassifiedMove& move) {
    int cpLoss = move.centipawn_loss;

    std::string label;
    std::string colorUnused;
    describeQuality(move.quality, label, colorUnused);
    // NOTE: previously this collapsed Brilliant/Best/Good into a single
    // "Good" label before handing it to the LLM. Since we now coach every
    // move (not just mistakes), the real label is kept so the prompt can
    // give properly-calibrated praise instead of flattening it.

    std::string bestMove = extractFirstMove(move.raw.engine_best_pv);

    std::string motifTag = "none";
    if (move.triggered_motifs.size() > 0) {
        motifTag = motifToString(move.triggered_motifs[0]);
    }

    std::string xml;
    xml += "<engine_snapshot>\n";
    xml += "  <eval_drop cp=\"" + std::to_string(cpLoss) + "\" label=\"" + label + "\"/>\n";
    xml += "  <best_move san=\"" + bestMove + "\" pv=\"" + move.raw.engine_best_pv + "\"/>\n";
    xml += "  <threat type=\"" + motifTag + "\"/>\n";
    xml += "  <motif>" + motifTag + "</motif>\n";
    xml += "</engine_snapshot>";
    return xml;
}

// Builds the full prompt that FastAPI will send to Gemini.
// FastAPI can use this directly or wrap it in its own system prompt.
inline std::string buildPrompt(const ClassifiedMove& move) {
    std::string xml = buildSnapshotXml(move);
    bool wasGood = (move.quality == BRILLIANT || move.quality == BEST || move.quality == GOOD);
    bool wasTopChoice = isTopEngineChoice(move);

    std::string prompt;
    prompt += "You are a warm, encouraging chess coach speaking directly to a student, ";
    prompt += "right after they made a move. The <engine_snapshot> below comes directly ";
    prompt += "from the chess engine and is ground truth. Do not invent moves, evaluations, ";
    prompt += "or threats that are not present in this snapshot. Do not repeat the raw XML ";
    prompt += "or numbers back verbatim - translate them into plain, human chess language.\n\n";
    prompt += xml;
    if (wasTopChoice) {
        prompt += "\n\n<note>The move played was the engine's #1 recommended move in this ";
        prompt += "position - there is no better alternative to mention.</note>";
    }
    prompt += "\n\nWork through these steps internally, then output ONLY the final coaching ";
    prompt += "note (2-4 sentences, plain conversational English, no headers, no bullet points, ";
    prompt += "no restating the eval number):\n";
    prompt += "1. Score Calculation: judge how significant this eval_drop/label is for the game.\n";
    prompt += "2. Geometric Translation: if a threat/motif is present, explain what it means in ";
    prompt += "plain chess terms (e.g. \"this opens the f-file toward your king\"), not coordinates.\n";
    prompt += "3. Pedagogical Synthesis: ";
    if (wasGood && wasTopChoice) {
        prompt += "This was literally the strongest move on the board - be genuinely enthusiastic ";
        prompt += "and specific about WHY it's strong: name the concrete thing it does (wins material, ";
        prompt += "defends a threat, improves a piece, seizes a key square/file, sets up a follow-up ";
        prompt += "plan, etc). Do not suggest any alternative move - there isn't a better one.\n";
    } else if (wasGood) {
        prompt += "Explain concretely WHY the move works, not just that it's good - name the specific ";
        prompt += "tactical or positional reason (what it defends, attacks, improves, or prevents). ";
        prompt += "Only mention best_move if it is meaningfully stronger than what was played; if you ";
        prompt += "do mention it, say briefly what makes it even better than the good move played.\n";
    } else {
        prompt += "explain what went wrong, why it matters for the position, and what ";
        prompt += "best_move/pv should have been played instead and why it's better.\n";
    }
    prompt += "Address the player directly as \"you\", vary your sentence openings across ";
    prompt += "different moves, and keep it specific to this position rather than generic advice.";
    return prompt;
}

// Fallback template used when Gemini fails or returns a bad response.
// FastAPI should call this as a fallback on its side too.
inline std::string fallbackNote(const ClassifiedMove& move) {
    bool wasGood = (move.quality == BRILLIANT || move.quality == BEST || move.quality == GOOD);

    if (wasGood && isTopEngineChoice(move)) {
        return "That was the engine's top choice in this position - well played!";
    }

    std::string bestMove = extractFirstMove(move.raw.engine_best_pv);
    std::string motifTag = (move.triggered_motifs.size() > 0)
        ? motifToString(move.triggered_motifs[0]) : "none";

    if (wasGood) {
        std::string note = "Good move (cp loss: " + std::to_string(move.centipawn_loss) + ").";
        if (bestMove != "None") note += " The engine's top pick here was " + bestMove + ".";
        return note;
    }

    std::string note = "This move lost " + std::to_string(move.centipawn_loss) + " centipawns. ";
    if (motifTag != "none") note += "The main issue was: " + motifTag + ". ";
    if (bestMove != "None") note += "Consider " + bestMove + " instead, following the line " + move.raw.engine_best_pv + ".";
    return note;
}

// Builds the single JSON package chess_bridge.exe prints to stdout after
// analyzing ONE move. This is the contract app.py's /analyze endpoint reads.
// engineMove: the bot's chosen reply (UCI), already computed by the caller.
//
// CHANGED: added "eval_cp" - the absolute position evaluation (from the
// mover's perspective) right after this move. Cp_loss alone only tells you
// how much was lost on THIS move, not where the game stands overall, which
// is what a live eval bar on the frontend actually needs to render.
inline std::string buildMoveAnalysisJson(const ClassifiedMove& move, const std::string& engineMove) {
    std::string label, color;
    describeQuality(move.quality, label, color);

    std::string bestMove = extractFirstMove(move.raw.engine_best_pv);

    std::string motifsJson = "[";
    for (int i = 0; i < (int)move.triggered_motifs.size(); i++) {
        motifsJson += "\"" + motifToString(move.triggered_motifs[i]) + "\"";
        if (i < (int)move.triggered_motifs.size() - 1) motifsJson += ", ";
    }
    motifsJson += "]";

    std::string j = "{\n";
    j += "  \"success\": true,\n";
    j += "  \"move_number\": " + std::to_string(move.raw.move_number) + ",\n";
    j += "  \"san\": \"" + jsonEscape(move.raw.san) + "\",\n";
    j += "  \"quality\": \"" + label + "\",\n";
    j += "  \"color\": \"" + color + "\",\n";
    j += "  \"cp_loss\": " + std::to_string(move.centipawn_loss) + ",\n";
    j += "  \"eval_cp\": " + std::to_string(move.raw.eval_after_cp) + ",\n";
    j += "  \"mover_is_white\": " + std::string(move.raw.is_white_to_move ? "true" : "false") + ",\n";
    j += "  \"motifs\": " + motifsJson + ",\n";
    j += "  \"is_top_engine_choice\": " + std::string(isTopEngineChoice(move) ? "true" : "false") + ",\n";
    j += "  \"best_alternative\": \"" + jsonEscape(bestMove) + "\",\n";
    j += "  \"best_alternative_pv\": \"" + jsonEscape(move.raw.engine_best_pv) + "\",\n";
    j += "  \"coaching_prompt\": \"" + jsonEscape(buildPrompt(move)) + "\",\n";
    j += "  \"fallback_note\": \"" + jsonEscape(fallbackNote(move)) + "\",\n";
    j += "  \"engine_move\": \"" + jsonEscape(engineMove) + "\"\n";
    j += "}";
    return j;
}

#endif
