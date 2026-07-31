#ifndef MODULE5_PERSONALIZER_H
#define MODULE5_PERSONALIZER_H

#include "chess_types.h"
#include <vector>
#include <cmath>
#include <algorithm>

struct MotifScore {
    int motifId;
    float score;
};

inline bool compareByScoreDescending(const MotifScore& a, const MotifScore& b) {
    return a.score > b.score;
}

struct PersonalizationResult {
    std::vector<float> new_profile;
    std::vector<float> sliders;
    std::vector<int> homework_motifs;
};

class PersonalizationEngine {
private:
    float alpha;
    int noise_filter_min_games;
    std::vector<std::vector<float>> W;
    std::vector<float> b;

public:
    PersonalizationEngine(float alphaValue = 0.15f, int noiseFilterGames = 4) {
        alpha = alphaValue;
        noise_filter_min_games = noiseFilterGames;

        std::vector<float> zeroRow(NUM_MOTIFS, 0.0f);
        W.assign(NUM_SLIDERS, zeroRow);
        b.assign(NUM_SLIDERS, 0.0f);

        // --- Weight matrix W ---
        // W[slider][motif] = how strongly that motif drives that slider.
        // Logic: if you have weakness X, which slider should respond and by how much?

        // King safety problems -> make bot MORE aggressive (player needs
        // to practice defending against attacks)
        W[AGGRESSION_BIAS][KING_SAFETY_COLLAPSE] = 2.8f;

        // Missed forks -> more tactical complexity (bot should set more
        // fork traps for the player to navigate)
        W[TACTICAL_COMPLEXITY][MISSED_FORK] = 2.0f;

        // Time pressure blunders -> also raise tactical complexity
        // (player needs to practice handling complex positions on the clock)
        W[TACTICAL_COMPLEXITY][TIME_PRESSURE_BLUNDER] = 1.9f;

        // Missed pins/skewers -> harder opponent (creates pin/skewer
        // situations more often by virtue of playing stronger moves)
        W[DIFFICULTY_CAP][MISSED_PIN_SKEWER] = 1.5f;

        // Back rank weakness -> endgame emphasis (back rank issues are
        // fundamentally an endgame technique problem)
        W[ENDGAME_EMPHASIS][BACK_RANK_WEAKNESS] = 1.7f;

        // Hanging pieces -> moderate difficulty increase (player needs
        // to practice calculating captures in all positions)
        W[DIFFICULTY_CAP][HANGING_PIECE] = 1.2f;

        // Tactical blindness (unexplained big drops) -> max tactical
        // complexity, the player is missing things they should see
        W[TACTICAL_COMPLEXITY][TACTICAL_BLINDNESS] = 2.2f;

        // Defensive inaccuracies -> slightly more aggression so the
        // player gets more practice defending difficult positions
        W[AGGRESSION_BIAS][DEFENSIVE_INACCURACY] = 1.3f;

        // Positional misjudgment -> contempt factor (make the bot play
        // for the win more, forcing player into complex strategic choices)
        W[CONTEMPT_FACTOR][POSITIONAL_MISJUDGMENT] = 1.1f;

        // Offensive miss -> mild tactical complexity increase
        W[TACTICAL_COMPLEXITY][OFFENSIVE_MISS] = 1.0f;

        // Default bias: slightly less aggressive so a clean-slate player
        // doesn't immediately face a wall of attacks
        b[AGGRESSION_BIAS] = -0.05f;
    }

    std::vector<float> updateProfile(const std::vector<float>& P_old, const std::vector<float>& x_t) {
        std::vector<float> P_new(NUM_MOTIFS);
        for (int i = 0; i < NUM_MOTIFS; i++) {
            P_new[i] = (1.0f - alpha) * P_old[i] + alpha * x_t[i];
        }
        return P_new;
    }

    // counts is modified in place - pass the loaded DB value, it gets
    // updated here and should be saved back to DB after this call.
    void updateConsecutiveCounts(std::vector<int>& counts, const std::vector<float>& x_t) {
        for (int i = 0; i < NUM_MOTIFS; i++) {
            if (x_t[i] > 0.0f) counts[i]++;
            else counts[i] = 0;
        }
    }

    std::vector<int> computeHomework(const std::vector<float>& P_new, const std::vector<int>& counts) {
        std::vector<MotifScore> list;
        for (int i = 0; i < NUM_MOTIFS; i++) {
            MotifScore item;
            item.motifId = i;
            item.score = P_new[i];
            list.push_back(item);
        }
        std::sort(list.begin(), list.end(), compareByScoreDescending);

        std::vector<int> homework;
        for (int i = 0; i < 3; i++) {
            int id = list[i].motifId;
            if (list[i].score > 0.0f && counts[id] >= noise_filter_min_games) {
                homework.push_back(id);
            }
        }
        return homework;
    }

    std::vector<float> computeSliders(const std::vector<float>& P) {
        std::vector<float> sliders(NUM_SLIDERS, 0.0f);
        for (int row = 0; row < NUM_SLIDERS; row++) {
            float total = b[row];
            for (int col = 0; col < NUM_MOTIFS; col++) {
                total += W[row][col] * P[col];
            }
            sliders[row] = (float)tanh(total);
        }
        return sliders;
    }

    PersonalizationResult runUpdate(const std::vector<float>& P_old,
                                     const std::vector<float>& x_t,
                                     std::vector<int>& counts) {
        PersonalizationResult result;
        result.new_profile = updateProfile(P_old, x_t);
        updateConsecutiveCounts(counts, x_t);
        result.homework_motifs = computeHomework(result.new_profile, counts);
        result.sliders = computeSliders(result.new_profile);
        return result;
    }
};

#endif
