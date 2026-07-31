#ifndef MODULE3_ANALYST_H
#define MODULE3_ANALYST_H

#include "chess_types.h"
#include <vector>

class GameAnalyst {
private:
    double time_pressure_seconds;

public:
    GameAnalyst(double threshold = 3.0) {
        time_pressure_seconds = threshold;
    }

    ClassifiedMove classifyMove(const RawMoveRecord& move) {
        ClassifiedMove result;
        result.raw = move;
        result.centipawn_loss = move.eval_before_cp - move.eval_after_cp;

        // Quality classification - thresholds from proposal Section 5.1
        if      (result.centipawn_loss <= 0)   result.quality = BRILLIANT;
        else if (result.centipawn_loss <= 10)  result.quality = BEST;
        else if (result.centipawn_loss <= 30)  result.quality = GOOD;
        else if (result.centipawn_loss <= 90)  result.quality = INACCURACY;
        else if (result.centipawn_loss <= 200) result.quality = MISTAKE;
        else                                   result.quality = BLUNDER;

        bool isBad = (result.quality == MISTAKE || result.quality == BLUNDER);
        bool isInaccuracy = (result.quality == INACCURACY);

        // --- Motif detection ---
        // These use the boolean flags from RawMoveRecord. Right now those
        // flags are set by heuristics in StockfishBridge. When your own
        // engine is ready, they'll be set by real bitboard analysis instead,
        // and these lines below stay exactly the same.

        if (move.left_piece_hanging) {
            result.triggered_motifs.push_back(HANGING_PIECE);
        }
        if (move.missed_fork_available) {
            result.triggered_motifs.push_back(MISSED_FORK);
        }
        if (move.king_safety_collapsed) {
            result.triggered_motifs.push_back(KING_SAFETY_COLLAPSE);
        }
        if (move.back_rank_weak) {
            result.triggered_motifs.push_back(BACK_RANK_WEAKNESS);
        }
        if (move.missed_pin_or_skewer) {
            result.triggered_motifs.push_back(MISSED_PIN_SKEWER);
        }

        // Time pressure: only counts when the move was also bad
        if (isBad && move.time_spent_seconds < time_pressure_seconds) {
            result.triggered_motifs.push_back(TIME_PRESSURE_BLUNDER);
        }

        bool hasSpecificMotif = (result.triggered_motifs.size() > 0);

        // Catch-all categories for bad moves with no specific pattern detected
        if (isBad && !hasSpecificMotif) {
            // DEFENSIVE_INACCURACY: bad move while the player was already
            // under pressure (eval was already negative before the move).
            // TACTICAL_BLINDNESS: bad move from a good position = missed something sharp.
            // POSITIONAL_MISJUDGMENT: smaller mistake, probably a slow strategic error.
            if (move.eval_before_cp < -50) {
                result.triggered_motifs.push_back(DEFENSIVE_INACCURACY);
            } else if (result.quality == BLUNDER) {
                result.triggered_motifs.push_back(TACTICAL_BLINDNESS);
            } else {
                result.triggered_motifs.push_back(POSITIONAL_MISJUDGMENT);
            }
        } else if (isInaccuracy && !hasSpecificMotif) {
            // Small loss from a good position = missed a slightly better option
            result.triggered_motifs.push_back(OFFENSIVE_MISS);
        }

        return result;
    }

    GameReport analyzeGame(const std::vector<RawMoveRecord>& moves) {
        GameReport report;
        report.motif_vector.assign(NUM_MOTIFS, 0.0f);
        report.phase_acpl.assign(3, 0.0);

        double totalCpLoss = 0.0;
        int blunders = 0, mistakes = 0, inaccuracies = 0, nearBest = 0;
        std::vector<int> motifCounts(NUM_MOTIFS, 0);
        std::vector<double> phaseCpSum(3, 0.0);
        std::vector<int> phaseMoveCount(3, 0);

        for (int i = 0; i < (int)moves.size(); i++) {
            ClassifiedMove cm = classifyMove(moves[i]);

            double loss = (cm.centipawn_loss > 0) ? (double)cm.centipawn_loss : 0.0;
            totalCpLoss += loss;

            int p = moves[i].phase;
            phaseCpSum[p] += loss;
            phaseMoveCount[p] += 1;

            if      (cm.quality == BLUNDER)              blunders++;
            else if (cm.quality == MISTAKE)              mistakes++;
            else if (cm.quality == INACCURACY)           inaccuracies++;
            else if (cm.quality == BRILLIANT || cm.quality == BEST) nearBest++;

            for (int j = 0; j < (int)cm.triggered_motifs.size(); j++) {
                motifCounts[cm.triggered_motifs[j]]++;
            }

            report.moves.push_back(cm);
        }

        int total = (int)moves.size();
        if (total > 0) {
            report.acpl = totalCpLoss / total;
            report.accuracy_pct = (double)nearBest / (double)total;
        } else {
            report.acpl = 0.0;
            report.accuracy_pct = 0.0;
        }

        report.blunders = blunders;
        report.mistakes = mistakes;
        report.inaccuracies = inaccuracies;

        for (int p = 0; p < 3; p++) {
            report.phase_acpl[p] = (phaseMoveCount[p] > 0)
                ? phaseCpSum[p] / phaseMoveCount[p] : 0.0;
        }

        if (total > 0) {
            for (int m = 0; m < NUM_MOTIFS; m++) {
                report.motif_vector[m] = (float)motifCounts[m] / (float)total;
            }
        }

        return report;
    }
};

#endif
