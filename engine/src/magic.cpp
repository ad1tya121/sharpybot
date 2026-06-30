#include "types.h"
#include "magic.h"
#include <vector>
#include <random>
#include <iostream>

Smagic mRookTable[64];
Smagic mBishopTable[64];
static U64 bishopMagicNums[64];
static U64 rookMagicNums[64];

// Rook total entries: 87,424 | Bishop total entries: 20,224
U64 attack_table[107648]; 

static const int rookBits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

static const int bishopBits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

static constexpr U64 rookMagics[64] = {
    0x18000c009108221ULL,0x40011000200840ULL,0x2a00208008120040ULL,0x100090024223000ULL,
    0x200280410020020ULL,0x100010004000812ULL,0x280110008800a00ULL,0x180004100003080ULL,
    0x18808008a0814009ULL,0x4c00020005000ULL,0x803004210a00500ULL,0x2040801004080080ULL,
    0x801001004180100ULL,0x204012004004810ULL,0x401800200010080ULL,0x20004008108c2ULL,
    0x8840008008318041ULL,0x400808040042008ULL,0x1480220042008012ULL,0x850808008021000ULL,
    0x481010010180025ULL,0x154004040020100ULL,0x400040010088562ULL,0x40020001812044ULL,
    0x808000c840002004ULL,0x40010100208440ULL,0x140324200208200ULL,0x4008000b80100180ULL,
    0xa001000500080010ULL,0x1480840080220080ULL,0x2080400100281ULL,0x8041801080284100ULL,
    0x1002804000800020ULL,0x2010002001400448ULL,0x26832000801001ULL,0x2011804806801000ULL,
    0x1008101501000801ULL,0x402000406001088ULL,0x209b00b04008208ULL,0x4000b07402000881ULL,
    0x400820041020024ULL,0x21000a0034dc000ULL,0x8402008050a20041ULL,0x8100008008080ULL,
    0x8c02000804620010ULL,0x204540002008080ULL,0x2000490810040012ULL,0x421000080410002ULL,
    0x80002082400080ULL,0xc10820220c10e00ULL,0x2585002000401100ULL,0x3004801002480080ULL,
    0x100080180040080ULL,0x2000408100200ULL,0x4a000c01684a00ULL,0x48600144830c0200ULL,
    0x4a110040248001ULL,0x508850018c00021ULL,0x8003001020004149ULL,0x2900019000421ULL,
    0x102003008842022ULL,0x60220010054c0882ULL,0x5840102209084084ULL,0x400040040810422ULL,
};

static constexpr U64 bishopMagics[64] = {
    0x80081080a0890200ULL,0x48110408008c2010ULL,0x1884010c01001008ULL,0x40080a0828040000ULL,
    0x42021008841180ULL,0x6021006000008ULL,0x4c0c0402088010ULL,0x102002108421000ULL,
    0x210814a002141520ULL,0x8000100210cc0080ULL,0xa040042164010008ULL,0x440401808100ULL,
    0x18c40422000000ULL,0x200020505600000ULL,0x4480401040a2010ULL,0x48144044008c5040ULL,
    0x4010002044108090ULL,0x5400282104240aULL,0x1110010204019020ULL,0x448048401202408ULL,
    0x802001012100062ULL,0x20001008a1708ULL,0x11050c04050404ULL,0x8023802200840908ULL,
    0x420102948100101ULL,0xc10420102c2800ULL,0x80c8020804140090ULL,0x2014004804010010ULL,
    0x840012802000ULL,0x5001020241024114ULL,0x101820801980442ULL,0x8002020040490080ULL,
    0x10086202040410ULL,0x9000940418501001ULL,0x405402604900401ULL,0x12040400880211ULL,
    0x10020080427004ULL,0x54080030820100ULL,0x2426040401010080ULL,0x2082200808040c0ULL,
    0x5108111029281000ULL,0x880319041020ULL,0x2205010880c1000ULL,0x4004c200900800ULL,
    0x40c5028800402ULL,0x28200c04102c20ULL,0x214145802048040ULL,0xc1840408820b0a22ULL,
    0x308421005600080ULL,0x71050110a20108ULL,0x42014404040000ULL,0x60020000940c0040ULL,
    0x20082b102021100ULL,0x101044810090002ULL,0x805620420a220014ULL,0xa854411404008400ULL,
    0x610308088084000ULL,0x40886008c040380ULL,0x80084a50541000ULL,0x808040609048800ULL,
    0x4000020204c40ULL,0x124030510820200ULL,0x48105c01080228ULL,0x8a1414940c040510ULL,
};


U64 computeRookMask(squareIndex square){
    const int currentFile = square % 8;
    const int currentRank = square / 8;
    U64 rookMask = 0ULL;
    //north
    for(int rank = currentRank + 1; rank < 7; rank++){
        int new_square = (rank * 8) + currentFile;
        rookMask |= 1ULL << new_square;}
    //south
    for(int rank = currentRank - 1; rank > 0; rank--){
        int new_square = (rank * 8) + currentFile;
        rookMask |= 1ULL << new_square;}
    //east
    for(int file = currentFile + 1; file < 7; file++){
        int new_square = (currentRank * 8) + file;
        rookMask |= 1ULL << new_square;}
    //west
    for(int file = currentFile - 1; file > 0; file--){
        int new_square = (currentRank * 8) + file;
        rookMask |= 1ULL << new_square;}
    return rookMask;
}

U64 computeBishopMask(squareIndex square){
    const int currentFile = square % 8;
    const int currentRank = square / 8;
    U64 bishopMask = 0ULL;
    //northeast
    for(int rank = currentRank + 1, file = currentFile + 1; rank < 7 && file < 7; rank++, file++){
        int new_square = (rank * 8) + file;
        bishopMask |= 1ULL << new_square;}
    //northwest   
    for(int rank = currentRank + 1, file = currentFile - 1; rank < 7 && file > 0; rank++, file--){
        int new_square = (rank * 8) + file;
        bishopMask |= 1ULL << new_square;}
    //southeast
    for(int rank = currentRank - 1, file = currentFile + 1; rank > 0 && file < 7; rank--, file++){
        int new_square = (rank * 8) + file;
        bishopMask |= 1ULL << new_square;}
    //southwest
    for(int rank = currentRank - 1, file = currentFile - 1; rank > 0 && file > 0; rank--, file--){
        int new_square = (rank * 8) + file;
        bishopMask |= 1ULL << new_square;}

    return bishopMask;
}


std::vector<U64> initBlockers(U64 mask){
    //vector initalization
    std::vector<U64> allBlockerBitBoards;
    //count bits and allocate the memory
    int maskBits = __builtin_popcountll(mask);
    allBlockerBitBoards.reserve(1ULL << maskBits);

    U64 blockers = 0ULL;
    do
    {
        allBlockerBitBoards.push_back(blockers);

        blockers = (blockers - mask) & mask;
    } while (blockers != 0ULL);
    
    return allBlockerBitBoards;
}



U64 computeRookAttacks(squareIndex square, U64 blockers){
    const int currentFile = square % 8;
    const int currentRank = square / 8;
    U64 attacks = 0ULL;
    //north
    for(int rank = currentRank + 1; rank < 8; rank++){
        int new_square = (rank * 8) + currentFile;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    //south
    for(int rank = currentRank - 1; rank >= 0; rank--){
        int new_square = (rank * 8) + currentFile;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    //east
    for(int file = currentFile + 1; file < 8; file++){
        int new_square = (currentRank * 8) + file;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    //west
    for(int file = currentFile - 1; file >= 0; file--){
        int new_square = (currentRank * 8) + file;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    return attacks;    
}

U64 computeBishopAttacks(squareIndex square, U64 blockers){
    const int currentFile = square % 8;
    const int currentRank = square / 8;
    U64 attacks = 0ULL;
    //northeast
    for(int rank = currentRank + 1, file = currentFile + 1; rank < 8 && file < 8; rank++, file++){
        int new_square = (rank * 8) + file;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    //northwest   
    for(int rank = currentRank + 1, file = currentFile - 1; rank < 8 && file >= 0; rank++, file--){
        int new_square = (rank * 8) + file;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    //southeast
    for(int rank = currentRank - 1, file = currentFile + 1; rank >= 0 && file < 8; rank--, file++){
        int new_square = (rank * 8) + file;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    //southwest
    for(int rank = currentRank - 1, file = currentFile - 1; rank >= 0 && file >= 0; rank--, file--){
        int new_square = (rank * 8) + file;
        U64 newSquareBit = 1ULL << new_square;
        attacks |= newSquareBit;
        if(newSquareBit & blockers){break;}
    }
    return attacks;
}


U64 findMagicNumber(squareIndex square, int maskBits, PieceType piece){
    U64 mask = (piece == ROOK) ? computeRookMask(square) : computeBishopMask(square);
    U64 magicNumber = 0ULL;
    U64 usedBlockers[4096];
    U64 usedAttacks[4096];

    std::random_device rd;
    std::mt19937_64 gen(rd());

    std::vector<U64> allBlockers = initBlockers(mask);
    std::vector<U64> allAttacks;
    allAttacks.reserve(allBlockers.size());

    for (U64 b : allBlockers) {
        allAttacks.push_back((piece == ROOK) ? computeRookAttacks(square, b) : computeBishopAttacks(square, b));
    }

    while (true)
    {   magicNumber = gen() & gen() & gen();

        bool failed = false;
    
        for(int i = 0; i < 4096; i++){
            usedAttacks[i] = 0ULL;
            usedBlockers[i] = 0ULL;}


        for (size_t i = 0; i < allBlockers.size(); i++){
            U64 b = allBlockers[i];
            U64 index = ( b * magicNumber ) >> (64 - maskBits);
            
            if (usedAttacks[index] == 0) {
                usedBlockers[index] = b;
                usedAttacks[index] = allAttacks[i];
            } 
            else if ( usedAttacks[index] != allAttacks[i] ) {
                failed = true;
                break;
            }
        }  
        
        if(!failed) {
            return magicNumber;
        }
    }    
    return magicNumber;
}


static void buildTable(Smagic table[64], const U64 magics[64], const int customBits[64],
                       U64 (*maskFn)(squareIndex),
                       U64 (*attackFn)(squareIndex, U64), 
                       U64* pool, int& offset)
{
    for(int sq = 0; sq < 64; sq++){
        U64 mask = maskFn((squareIndex)sq);

        int bits = customBits[sq];

        int size = 1 << bits;

        table[sq].mask = mask;
        table[sq].magic = magics[sq];
        table[sq].shift = 64 - bits;

        // give this square a pointer to the exact starting address inside our glbal pool
        table[sq].ptr = pool + offset;

        offset += size;

        std::vector<U64> blockers = initBlockers(mask);
        

        for (U64 occ : blockers) {
            unsigned int idx = (unsigned int)((occ * magics[sq]) >> table[sq].shift);
            table[sq].ptr[idx] = attackFn((squareIndex)sq, occ); 
        }
    }
}

void initMagicTables(){
    int offset = 0;

    for (int sq = 0; sq < 64; sq++) {
        rookMagicNums[sq] = rookMagics[sq];
    }
    buildTable(mRookTable, rookMagicNums, rookBits, 
            computeRookMask, computeRookAttacks, 
            attack_table, offset);

    for (int sq = 0; sq < 64; sq++) {
        bishopMagicNums[sq] = bishopMagics[sq];
    }
    buildTable(mBishopTable, bishopMagicNums, bishopBits, 
            computeBishopMask, computeBishopAttacks, 
            attack_table, offset);
}
