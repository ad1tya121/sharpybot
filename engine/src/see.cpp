#include "types.h"
#include "attacks.h"
#include "magic.h"
#include "see.h"

U64 attackersTo(const Board& board, int sq, U64 occupied) {
    U64 attackers = 0;

    attackers |= pawnAttacks[BLACK][sq] & board.pieces[WHITE][PAWN]; // white pawns attacking sq
    attackers |= pawnAttacks[WHITE][sq] & board.pieces[BLACK][PAWN]; // black pawns attacking sq

    attackers |= knightAttacks[sq] & (board.pieces[WHITE][KNIGHT] | board.pieces[BLACK][KNIGHT]);
    attackers |= kingAttacks[sq]   & (board.pieces[WHITE][KING]   | board.pieces[BLACK][KING]);

    U64 bishopAtk = bishopAttacks(sq, occupied);
    U64 rookAtk   = rookAttacks(sq, occupied);

    attackers |= bishopAtk & (board.pieces[WHITE][BISHOP] | board.pieces[BLACK][BISHOP] |
                               board.pieces[WHITE][QUEEN]  | board.pieces[BLACK][QUEEN]);
    attackers |= rookAtk   & (board.pieces[WHITE][ROOK]   | board.pieces[BLACK][ROOK]   |
                               board.pieces[WHITE][QUEEN]  | board.pieces[BLACK][QUEEN]);

    return attackers & occupied;
}

PieceType movingPieceAt(const Board& board, Color side, int sq) {
    for (int p = 0; p < 6; p++)
        if (getBit(board.pieces[side][p], sq)) return (PieceType)p;
    return NONE;
}

bool seeGE(const Board& board, Move move, int threshold) {
    if (move.captured == NONE) return threshold <= 0; // not a capture, nothing to gain

    Color us = board.sideToMove;
    PieceType nextVictim = movingPieceAt(board, us, move.StartSquare);
    PieceType captured   = move.captured;

    int balance = SEE_VALUE[captured] - threshold;
    if (balance < 0) return false; // even a free capture doesn't reach threshold

    balance -= SEE_VALUE[nextVictim];
    if (balance >= 0) return true; // still good even losing our attacker for free

    U64 occupied = board.occupied;
    clearBit(occupied, move.StartSquare);
    setBit(occupied, move.TargetSquare);

    if (move.isEnpassant) {
        int capSq = move.TargetSquare + (us == WHITE ? -8 : 8);
        clearBit(occupied, capSq);
    }

    U64 attackers = attackersTo(board, move.TargetSquare, occupied);
    Color side = (us == WHITE) ? BLACK : WHITE; // opponent recaptures first

    while (true) {
        U64 sideAttackers = attackers & occupied &
            (board.pieces[side][PAWN]   | board.pieces[side][KNIGHT] | board.pieces[side][BISHOP] |
             board.pieces[side][ROOK]   | board.pieces[side][QUEEN]  | board.pieces[side][KING]);
        if (!sideAttackers) break; // no more attackers for this side

        PieceType pt = NONE;
        int sq = -1;
        for (int p = PAWN; p <= KING; p++) {
            U64 bb = sideAttackers & board.pieces[side][p];
            if (bb) { pt = (PieceType)p; sq = getLSB(bb); break; }
        }
        if (pt == NONE) break;

        clearBit(occupied, sq);
        clearBit(attackers, sq);
        attackers |= attackersTo(board, move.TargetSquare, occupied) & occupied; // reveal x-rays

        nextVictim = pt;
        side = (side == WHITE) ? BLACK : WHITE;

        balance = -balance - 1 - SEE_VALUE[nextVictim];
        if (balance >= 0) {
            if (nextVictim == KING) {
                U64 opponentAttackers = attackers & occupied &
                    (board.pieces[side][PAWN]   | board.pieces[side][KNIGHT] | board.pieces[side][BISHOP] |
                     board.pieces[side][ROOK]   | board.pieces[side][QUEEN]  | board.pieces[side][KING]);
                if (opponentAttackers) side = (side == WHITE) ? BLACK : WHITE; // king can't capture into check
            }
            break;
        }
    }

    return us != side;
}