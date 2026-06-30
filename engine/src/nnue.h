#pragma once
#include "types.h"
#include "board.h"
#include <array>

constexpr int INPUT_SIZE   = 40960;
constexpr int H1_SIZE = 256;
constexpr int H2_SIZE = 32;
constexpr int H3_SIZE = 32;
constexpr int SCALE = 64;
constexpr float OUT_SCALE = 600.0f;

struct Network {
    // feature transformer weights and biases
    int16_t featureT_weights[INPUT_SIZE][H1_SIZE];
    int16_t featureT_biases[H1_SIZE];
    // hidden layer 1 weights and biases
    int16_t hidden1_weights[2 * H1_SIZE][H2_SIZE];
    int16_t hidden1_biases[H2_SIZE];
    // hidden layer 2 weights and biases
    int16_t hidden2_weights[H2_SIZE][H3_SIZE];
    int16_t hidden2_biases[H3_SIZE];
    // output weights and biase
    int16_t output_weights[H3_SIZE];
    int32_t output_bias;
};

struct Accumulator {
    int16_t whiteValues[H1_SIZE];
    int16_t blackValues[H1_SIZE];
};

extern Network net;
extern Accumulator accStack[512];

uint16_t halfKpIndex(int king_square, int piece_square, PieceType piece_type, Color piece_color);
void initAccumulator(const Board& board, Accumulator& acc);
int32_t nnueEval(const Board& board, int ply);
bool loadNetwork(const char* path);
void updateAccumulator(const Board& board, int ply, Move move);
