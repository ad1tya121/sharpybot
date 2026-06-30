#include "zobrist.h"
#include "board.h"
#include <random>

ZobristData zobrist;

void initZobrist(){
    std::mt19937_64 prng(123456789ULL);

    for(int piece = 0; piece < 12; piece++){
        for(int square = 0; square < 64; square++){
            zobrist.z_pieces[piece][square] = prng();
        }
    }
// total castling rights combination = 2**4 = 16, since there are 4 castling rights and either of them could be True and False. 
    for(int totalComb = 0; totalComb < 16; totalComb++){
        zobrist.z_castling[totalComb] = prng();
    }

    for(int file = 0; file < 8; file++){
        zobrist.z_enPassant[file] = prng();
    }

    zobrist.z_side = prng();
}

