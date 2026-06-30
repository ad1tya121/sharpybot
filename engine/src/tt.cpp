#include "tt.h"
#include "types.h"
#include "search.h"
#include <cstring>

// helper function which is just one time help xd
uint16_t to_uint16(const Move& move) {
    uint16_t raw = 0;
    raw |= (move.StartSquare & 0x3F);          // Bits 0-5: Start Square
    raw |= ((move.TargetSquare & 0x3F) << 6);  // Bits 6-11: Target Square
    
    // Bits 12-15: Flags
    if (move.isEnpassant) {
        raw |= (1 << 12);
    } else if (move.isCastle) {
        raw |= (2 << 12);
    } else if (move.promoted != PieceType::NONE) {
        // Map each promotion piece type to a unique 4-bit ID (from 3 to 6)
        switch (move.promoted) {
            case PieceType::KNIGHT: raw |= (3 << 12); break;
            case PieceType::BISHOP: raw |= (4 << 12); break;
            case PieceType::ROOK:   raw |= (5 << 12); break;
            case PieceType::QUEEN:  raw |= (6 << 12); break;
            default: break;
        }
    }
    return raw;
}

Move from_uint16(uint16_t raw) {
    Move move{};
    move.StartSquare = raw & 0x3F;
    move.TargetSquare = (raw >> 6) & 0x3F;
    
    uint8_t flags = (raw >> 12) & 0x0F;
    if (flags == 1) move.isEnpassant = true;
    else if (flags == 2) move.isCastle = true;
    else if (flags >= 3) {
        switch (flags) {
            case 3: move.promoted = PieceType::KNIGHT; break;
            case 4: move.promoted = PieceType::BISHOP; break;
            case 5: move.promoted = PieceType::ROOK;   break;
            case 6: move.promoted = PieceType::QUEEN;  break;
        }
    }
    return move;
}


// calculating now many entries can tt store and assigning a dynamic array to store that
TranspositionTable::TranspositionTable(size_t megabytes){
    size_t totalBytes = megabytes * 1024 * 1024;
    no_of_entries = totalBytes / sizeof(TTEntry);
    TT = new TTEntry[no_of_entries];
}

// free array memory
TranspositionTable::~TranspositionTable(){
    delete[] TT;
}

// clear the hash key before new game # garbage
void TranspositionTable::clear(){
    std::memset(TT, 0, no_of_entries * sizeof(TTEntry));
}

// if this hash is already stored just fetch the info and skip the search
bool TranspositionTable::probeHash(U64 hashKey, int depth, int& score, int alpha, int beta, Move& bestMove){
    size_t index = hashKey % no_of_entries;

    if(TT[index].hashKey == hashKey){
        uint16_t rawMove = TT[index].bestMove;
        bestMove = from_uint16(rawMove);

        if(TT[index].depth >= depth){
            if (TT[index].flag == HASH_EXACT) {
                score = TT[index].score;
                return true;
            }
            
            if (TT[index].flag == HASH_ALPHA && TT[index].score <= alpha) {
                score = TT[index].score;
                return true;
            }
            
            if (TT[index].flag == HASH_BETA && TT[index].score >= beta) {
                score = TT[index].score;
                return true;
            }
        }
    }
    return false;
}

// store the info for later calc
void TranspositionTable::storeHash(U64 hashKey, int depth, int score, uint8_t flag, Move bestMove){
    size_t index = hashKey % no_of_entries;

    if(TT[index].hashKey == 0 || 
        TT[index].hashKey == hashKey ||
        depth >= TT[index].depth)
        {
        TT[index].hashKey = hashKey;
        TT[index].depth = depth;
        TT[index].score = score;
        TT[index].flag = flag;
        TT[index].bestMove = to_uint16(bestMove);                
    }
}

int scoreToTT(int score, int ply) {
    if (score >= mateScore - 1000) return score + ply;
    if (score <= -mateScore + 1000) return score - ply;
    return score;
}

int scoreFromTT(int score, int ply) {
    if (score >= mateScore - 1000) return score - ply;
    if (score <= -mateScore + 1000) return score + ply;
    return score;
}

