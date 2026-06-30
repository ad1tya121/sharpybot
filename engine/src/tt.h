#pragma once
#include "types.h"
#include "board.h"

enum HashFlags {HASH_EXACT, HASH_ALPHA, HASH_BETA};

struct TTEntry {
    U64 hashKey;
    int32_t score;
    uint16_t bestMove;
    uint8_t depth;
    uint8_t flag;
};

class TranspositionTable {
private:
    TTEntry* TT;
    size_t no_of_entries;
public:
    TranspositionTable(size_t megabytes);
    bool probeHash(U64 hashKey, int depth, int& score, int alpha, int beta, Move& bestMove);
    void storeHash(U64 hashKey, int depth, int score, uint8_t flag, Move bestMove);
    void clear();
    ~TranspositionTable();
};

int scoreToTT(int score, int ply);
int scoreFromTT(int score, int ply);
uint16_t to_uint16(const Move& move);
Move from_uint16(uint16_t raw);
extern TranspositionTable TT;
