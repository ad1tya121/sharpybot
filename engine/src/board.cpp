#include "board.h"
#include "zobrist.h"
#include "evaluation.h"
#include <iostream>
#include <sstream>
#include <cctype>

Board::Board() {
// Clear all piece bitboards
    for(int c = 0; c < 2; c++)
        for(int p = 0; p < 6; p++)
            pieces[c][p] = 0ULL;

    sideToMove = WHITE;
    castlingRights = 0;
    enPassantSquare = 64;
    halfMoveClock = 0;
    historyIdx = 0;

    updateOccupancy();
}


void Board::print_bitboard(U64 bitboard) const {
    for(int rank = 7; rank >= 0; rank--){
        std::cout << rank + 1 << " ";
 
        for(int file = 0; file < 8; file++){
            int square = rank * 8 + file;
                
            // Check if the bit at 'square' is set to 1
            if ((bitboard >> square) & 1ULL) {
                std::cout << "1 ";
            } else {
                std::cout << ". ";
            }
        }
            // print new line every rank
            std::cout << '\n';
    }
    std::cout << "\n  A B C D E F G H\n\n";
}   


void Board::updateOccupancy() {
    occupied_white = 0ULL;
    occupied_black = 0ULL;
    for(int p = 0; p < 6; p++){
        occupied_white |= pieces[WHITE][p];
        occupied_black |= pieces[BLACK][p];
    }
    occupied = occupied_black | occupied_white;
    empties = ~occupied;
}

void loadPosition(Board& board, std::string fen){
    //empty the board
    for(int c = 0; c < 2; c++)
        for(int p = 0; p < 6; p++)
            board.pieces[c][p] = 0ULL;

    int rank = 7, file =0;
    std::stringstream ss(fen);
    std::string piecePart, turn, castling, ep;        
    ss >> piecePart >> turn >> castling >> ep;

    for(char c : piecePart){
        if(c == '/') { file = 0; rank--; }
        else if (isdigit(c)) file += (c - '0');
        else {
            int color = isupper(c) ? WHITE : BLACK;
            int piece;
            switch (tolower(c)) {
                case 'p': piece = PAWN;   break;
                case 'n': piece = KNIGHT; break;
                case 'b': piece = BISHOP; break;
                case 'r': piece = ROOK;   break;
                case 'q': piece = QUEEN;  break;
                case 'k': piece = KING;   break;
            } 
            setBit(board.pieces[color][piece], rank * 8 + file);
            file++;
        }
    }
    //turn
    board.sideToMove = (turn == "w") ? WHITE : BLACK;
    //castling
    board.castlingRights = 0;
    if(castling.find('K') != std::string::npos) board.castlingRights |= WK_CASTLE;
    if(castling.find('Q') != std::string::npos) board.castlingRights |= WQ_CASTLE;
    if(castling.find('k') != std::string::npos) board.castlingRights |= BK_CASTLE;
    if(castling.find('q') != std::string::npos) board.castlingRights |= BQ_CASTLE;
    //ep
    board.enPassantSquare = (ep == "-") ? 64 : (ep[0] - 'a') + (ep[1] - '1') * 8;
    //update
    board.updateOccupancy();
    board.gamePhase = getGamePhase(board);
//setup the zobrist hashing
    U64 temp_hash = 0ULL;

    for(int c = 0; c < 2; c++){
        for(int p = 0; p < 6; p++){
            for(int square = 0; square < 64; square++){
                if(getBit(board.pieces[c][p], square)) {
                    int zobristPieceIndex = (c * 6) + p;
                    temp_hash ^= zobrist.z_pieces[zobristPieceIndex][square];
                }
            }
        }
    }

    temp_hash ^= zobrist.z_castling[board.castlingRights];

    if(board.enPassantSquare != 64){
        int file = board.enPassantSquare % 8;
        temp_hash ^= zobrist.z_enPassant[file];
    }

    if(board.sideToMove == BLACK){
        temp_hash ^= zobrist.z_side;
    }
    
    board.hash = temp_hash;
}

void loadStartPosition(Board& board){
    loadPosition(board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

int getGamePhase(const Board& board) {
    int gamePhase = 0;
    for (int p = 0; p < 6; p++) {
        int count = countBits(board.pieces[WHITE][p]) + countBits(board.pieces[BLACK][p]);
        gamePhase += count * gamephaseInc[p];
    }
    if (gamePhase > 24) gamePhase = 24;
    return gamePhase;
}
