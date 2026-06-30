#pragma once
#include "types.h"
#include <string>

// Move encoding
struct Move
{
    int StartSquare;
    int TargetSquare;
    PieceType captured; // castling, promotion, en passant etc.
    PieceType promoted;
    bool isEnpassant;
    bool isCastle;

    bool operator==(const Move& other) const = default;
    bool operator!=(const Move& other) const { return !(*this == other); }
};

struct MoveList {
    Move moves[256];
    int count = 0;

    void add(const Move& move) {
        moves[count++] = move;
    }
};


// StateInfo - saved before each move for unmakeMove
struct BoardState {
    uint8_t castlingRights;
    int enPassantSquare;
    uint8_t halfMoveClock;
    PieceType capturedPiece;
    U64 hash;
    int gamePhase;
};

struct NullMoveUndo {
    int enPassantSquare;
    uint8_t castlingRights;
    uint8_t halfMoveClock;
    U64 hash;
};

struct Board {
    // Board
    U64 pieces[2][6];
    U64 occupied_white, occupied_black, occupied, empties;

    // current game state
    Color sideToMove;
    uint8_t castlingRights;
    int enPassantSquare;
    uint8_t halfMoveClock;
    U64 hash;
    int gamePhase;
    // history
    BoardState history[512];
    int historyIdx;

    Board();
    void updateOccupancy();
    void print_bitboard(U64 bitboard) const;
};

void loadPosition(Board& board, std::string fen);
void loadStartPosition(Board& board);
int getGamePhase(const Board& board);
