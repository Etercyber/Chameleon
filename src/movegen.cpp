/*
  Chameleon, a UCI Chinese Chess (also called Xiangqi) engine derived from Stockfish 7
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2017 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2017-2019 Wilbert Lee

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "bitcount.h"
#include "init.h"

#define SERIALIZE(B, F) while(B) *moveList++ = make_move(F, pop_lsb(&B))
#define SERIALIZE2(B, D) while(B) { Square to = pop_lsb(&B); Square from = to - (D); *moveList++ = make_move(from, to); }
#define SERIALIZE3(B, D) while(B) { Square to = pop_lsb(&B); Square from = to - (D); if (from == exclued)  continue; *moveList++ = make_move(from, to); }

static const char* PieceMap[COLOR_NB] = { " PABNCRK", " pabncrk" };

template<PieceType Pt>
static ExtMove* generate_moves(const Position& pos, ExtMove* moveList)
{
  assert(Pt != CANNON && Pt != PAWN);

  Color us = pos.side_to_move();

  const Square* pl = pos.squares<Pt>(us);

  Bitboard att;
  Bitboard target = ~pos.pieces(us);

  for (Square from = *pl; from != SQ_NONE; from = *++pl)
  {
    att = pos.attacks_from<Pt>(from) & target;

    SERIALIZE(att, from);
  }
  return moveList;
}

template<>
static ExtMove* generate_moves<CANNON>(const Position& pos, ExtMove* moveList)
{
  Color us = pos.side_to_move();

  const Square* pl = pos.squares<CANNON>(us);

  Bitboard att, natt;
  Bitboard empty = ~pos.pieces();
  Bitboard target = ~pos.pieces(us);

  for (Square from = *pl; from != SQ_NONE; from = *++pl)
  {
    att = pos.attacks_from<CANNON>(from) & target;
    natt = pos.attacks_from<ROOK>(from) & empty;

    SERIALIZE(att, from);
    SERIALIZE(natt, from);
  }
  return moveList;
}

static ExtMove* generate_pawn_moves(const Position& pos, ExtMove* moveList)
{
  Color    us = pos.side_to_move();
  Bitboard target = ~pos.pieces(us);
  Bitboard pawns = pos.pieces(us, PAWN);

  const Square Up = (us == WHITE ? DELTA_N : DELTA_S);
  const Square Right = (us == WHITE ? DELTA_E : DELTA_W);
  const Square Left = (us == WHITE ? DELTA_W : DELTA_E);
  const Bitboard MaskBB = PawnMask[us];

  Bitboard attup = shift_bb(pawns, Up) & MaskBB & target;
  Bitboard attleft = shift_bb(pawns, Left) & MaskBB & target;
  Bitboard attright = shift_bb(pawns, Right) & MaskBB & target;

  SERIALIZE2(attup, Up);
  SERIALIZE2(attleft, Left);
  SERIALIZE2(attright, Right);

  return moveList;
}

static ExtMove* gen_all_moves(const Position& pos, ExtMove* moveList)
{
  moveList = generate_moves<ROOK>(pos, moveList);
  moveList = generate_moves<KNIGHT>(pos, moveList);
  moveList = generate_moves<CANNON>(pos, moveList);
  moveList = generate_pawn_moves(pos, moveList);
  moveList = generate_moves<BISHOP>(pos, moveList);
  moveList = generate_moves<ADVISOR>(pos, moveList);
  moveList = generate_moves<KING>(pos, moveList);

  return moveList;
}

bool move_is_legal(const Position& pos, Move move)
{
  assert(is_ok(move));

  Move m = move;
  Color us = pos.side_to_move();
  Color them = (us == WHITE ? BLACK : WHITE);
  Square from = from_sq(m);
  Square to = to_sq(m);

  assert(color_of(pos.moved_piece(m)) == us);
  assert(pos.piece_on(pos.square<KING>(us)) == make_piece(us, KING));

  PieceType pfr = type_of(pos.piece_on(from));
  PieceType pto = type_of(pos.piece_on(to));

  Bitboard pawns = pos.pieces(them, PAWN);
  Bitboard knights = pos.pieces(them, KNIGHT);
  Bitboard cannons = pos.pieces(them, CANNON);
  Bitboard rooks = pos.pieces(them, ROOK);
  Bitboard occ = pos.pieces();

  occ ^= from;
  if (pto == NO_PIECE_TYPE)
    occ ^= to;
  Square ksq = pos.square<KING>(us);
  if (ksq == from)
    ksq = to;

  if (pto != NO_PIECE_TYPE)
  {
    switch (pto)
    {
    case PAWN:
      pawns ^= to;
      break;
    case KNIGHT:
      knights ^= to;
      break;
    case ROOK:
      rooks ^= to;
      break;
    case CANNON:
      cannons ^= to;
      break;
    }
  }

  if ((RookAttackMask[ksq] & cannons) && (cannon_attacks_bb(ksq, occ) & cannons))
    return false;
  if ((RookAttackMask[ksq] & rooks) && (rook_attacks_bb(ksq, occ) & rooks))
    return false;
  if ((KnightAttackMask[ksq] & knights) && (knight_eye_attacks_bb(ksq, occ) & knights))
    return false;
  if ((PawnAttackTo[them][ksq] & pawns))
    return false;
  if ((RookAttackMask[ksq] & pos.square<KING>(them)) && (rook_attacks_bb(ksq, occ) & pos.square<KING>(them)))
    return false;

  return true;
}

bool move_is_check(const Position& pos, Move move)
{
  Color us = pos.side_to_move();
  Square from = from_sq(move);
  Square to = to_sq(move);
  Square ksq = pos.square<KING>(~us);

  PieceType pfr = type_of(pos.piece_on(from));
  PieceType pto = type_of(pos.piece_on(to));

  Bitboard pawns = pos.pieces(us, PAWN);
  Bitboard knights = pos.pieces(us, KNIGHT);
  Bitboard cannons = pos.pieces(us, CANNON);
  Bitboard rooks = pos.pieces(us, ROOK);
  Bitboard occ = pos.pieces();

  occ ^= from;
  if (pto == NO_PIECE_TYPE)
    occ ^= to;

  switch (pfr)
  {
  case ROOK:
    rooks ^= from;
    rooks ^= to;
    break;
  case CANNON:
    cannons ^= from;
    cannons ^= to;
    break;
  case KNIGHT:
    knights ^= from;
    knights ^= to;
    break;
  case PAWN:
    pawns ^= from;
    pawns ^= to;
    break;
  default:
    break;
  }

  if ((RookAttackMask[ksq] & cannons) && (cannon_attacks_bb(ksq, occ) & cannons)) return true;
  if ((RookAttackMask[ksq] & rooks) && (rook_attacks_bb(ksq, occ) & rooks)) return true;
  if ((KnightAttackMask[ksq] & knights) && (knight_eye_attacks_bb(ksq, occ) & knights)) return true;
  if ((PawnAttackTo[us][ksq] & pawns)) return true;

  return false;
}

template<Color Us, GenType Type>
ExtMove* generate_pawn_moves(const Position& pos,
  ExtMove* moveList, Bitboard target, Square exclued, const CheckInfo* ci)
{
  Color us = Us;
  Bitboard pawns = pos.pieces(us, PAWN);

  const Square   Up = (us == WHITE ? DELTA_N : DELTA_S);
  const Square   Right = (us == WHITE ? DELTA_E : DELTA_W);
  const Square   Left = (us == WHITE ? DELTA_W : DELTA_E);
  const Bitboard MaskBB = PawnMask[us];

  Bitboard attup = shift_bb(pawns, Up) & MaskBB & target;
  Bitboard attleft = shift_bb(pawns, Left) & MaskBB & target;
  Bitboard attright = shift_bb(pawns, Right) & MaskBB & target;

  SERIALIZE3(attup, Up);
  SERIALIZE3(attleft, Left);
  SERIALIZE3(attright, Right);

  return moveList;
}

template<PieceType Pt>
ExtMove* generate_moves(const Position& pos, ExtMove* moveList,
  Color us, Bitboard target, Square exclued, const CheckInfo* ci)
{
  assert(Pt != KING && Pt != CANNON && Pt != PAWN);

  const Square* pl = pos.squares<Pt>(us);

  Bitboard att;

  for (Square from = *pl; from != SQ_NONE; from = *++pl)
  {
    if (from == exclued)  continue;

    att = pos.attacks_from<Pt>(from) & target;

    SERIALIZE(att, from);
  }
  return moveList;
}

template<>
ExtMove* generate_moves<CANNON>(const Position& pos, ExtMove* moveList,
  Color us, Bitboard target, Square exclued, const CheckInfo* ci)
{
  const Square* pl = pos.squares<CANNON>(us);

  Bitboard att, natt;
  Bitboard empty = ~pos.pieces();

  for (Square from = *pl; from != SQ_NONE; from = *++pl)
  {
    if (from == exclued)  continue;

    att = pos.attacks_from<CANNON>(from) & target & pos.pieces(~us);

    SERIALIZE(att, from);

    natt = pos.attacks_from<ROOK>(from) & empty & target;

    SERIALIZE(natt, from);
  }
  return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_all(const Position& pos, ExtMove* moveList,
  Bitboard target, Square exclued, const CheckInfo* ci = nullptr)
{
  const bool Checks = Type == QUIET_CHECKS;

  moveList = generate_moves<  ROOK>(pos, moveList, Us, target, exclued, ci);
  moveList = generate_moves<CANNON>(pos, moveList, Us, target, exclued, ci);
  moveList = generate_moves<KNIGHT>(pos, moveList, Us, target, exclued, ci);
  moveList = generate_pawn_moves<Us, Type>(pos, moveList, target, exclued, ci);
  moveList = generate_moves<BISHOP>(pos, moveList, Us, target, exclued, ci);
  moveList = generate_moves<ADVISOR>(pos, moveList, Us, target, exclued, ci);

  if (Type != EVASIONS)
  {
    Square ksq = pos.square<KING>(Us);
    Bitboard att = pos.attacks_from<KING>(ksq) & target;

    SERIALIZE(att, ksq);
  }
  return moveList;
}

/// generate<CAPTURES> generates all pseudo-legal captures
/// promotions. Returns a pointer to the end of the move list.
///
/// generate<QUIETS> generates all pseudo-legal non-captures and
/// underpromotions. Returns a pointer to the end of the move list.
///
/// generate<NON_EVASIONS> generates all pseudo-legal captures and
/// non-captures. Returns a pointer to the end of the move list.

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList)
{
  assert(Type == CAPTURES || Type == QUIETS || Type == NON_EVASIONS);
  assert(!pos.checkers());

  Color us = pos.side_to_move();
  Bitboard target = Type == CAPTURES ? pos.pieces(~us)
    : Type == QUIETS ? ~pos.pieces()
    : Type == NON_EVASIONS ? ~pos.pieces(us) : Bitboard();

  return us == WHITE ? generate_all<WHITE, Type>(pos, moveList, target, SQ_NONE)
    : generate_all<BLACK, Type>(pos, moveList, target, SQ_NONE);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);

/// generate<QUIET_CHECKS> generates all pseudo-legal non-captures and knight
/// underpromotions that give check. Returns a pointer to the end of the move list.

template<>
ExtMove* generate<QUIET_CHECKS>(const Position& pos, ExtMove* moveList)
{
  assert(!pos.checkers());

  Color us = pos.side_to_move();
  CheckInfo ci(pos);
  Bitboard dc = ci.dcCandidates;
  Bitboard cannonsforbid = pos.discovered_cannon_face_king();
  ExtMove* cur = moveList;

  // Because of the problems with cannon, the problems are complicated
  // There is room for further optimization
  moveList = us == WHITE ? generate_all<WHITE, QUIET_CHECKS>(pos, moveList, ~pos.pieces(), SQ_NONE, &ci)
    : generate_all<BLACK, QUIET_CHECKS>(pos, moveList, ~pos.pieces(), SQ_NONE, &ci);

  while (cur != moveList)
  {
    if (move_is_check(pos, cur->move))
      ++cur;
    else
      *cur = (--moveList)->move;
  }
  return moveList;
}


/// generate<EVASIONS> generates all pseudo-legal check evasions when the side
/// to move is in check. Returns a pointer to the end of the move list.

template<>
ExtMove* generate<EVASIONS>(const Position& pos, ExtMove* moveList)
{
  assert(pos.checkers());

  Color us = pos.side_to_move();
  Square ksq = pos.square<KING>(us);
  Bitboard b;
  Bitboard target;
  Bitboard sliderAttacks;
  Bitboard sliders = pos.checkers() & ~pos.pieces(KNIGHT, PAWN);
  Bitboard occ = pos.pieces();

  // Find all the squares attacked by slider checkers. We will remove them from
  // the king evasions in order to skip known illegal moves, which avoids any
  // useless legality checks later on.
  while (sliders)
  {
    Square checksq = pop_lsb(&sliders);

    switch (type_of(pos.piece_on(checksq)))
    {
    case ROOK:
      sliderAttacks |= RookAttackMask[checksq];
      break;
    case CANNON:
      sliderAttacks |= cannon_attacks_bb(checksq, occ);
      sliderAttacks |= suppercannon_attacks_bb(checksq, occ);
      break;
    default:
      break;
    }
  }

  b = pos.attacks_from<KING>(ksq) & ~pos.pieces(us) & ~sliderAttacks;
  while (b)
    *moveList++ = make_move(ksq, pop_lsb(&b));

  if (pos.checkers().more_than_one())
  {
    assert(popcount(pos.checkers()) > 1);

    ExtMove* cur = moveList;
    ExtMove* end = us == WHITE ? generate_all<WHITE, EVASIONS>(pos, moveList, ~pos.pieces(us), SQ_NONE)
      : generate_all<BLACK, EVASIONS>(pos, moveList, ~pos.pieces(us), SQ_NONE);

    // That's a problem: Many will, can only file
    // The intersection of multiple files is not correct
    // The union of multiple files, and then according to the set generation method, the set range will not be very large
    // Because many will, the situation is very rare, can produce all legal action law set
    while (cur != end)
    {
      if (!move_is_legal(pos, cur->move))
        *cur = (--end)->move;
      else
        ++cur;
    }
    return end;
  }
  else
  {
    assert(popcount(pos.checkers()) == 1);

    Square checksq = lsb(pos.checkers());
    PieceType pt = type_of(pos.piece_on(checksq));
    if (pt == CANNON)
    {
      Bitboard att;
      Bitboard natt;
      Square midsq = lsb(between_bb(checksq, ksq) & occ);
      if (color_of(pos.piece_on(midsq)) == us)
      {
        PieceType pt2 = type_of(pos.piece_on(midsq));
        switch (pt2)
        {
        case ROOK:
          att = pos.attacks_from<ROOK>(midsq) & ~pos.pieces(us) & (~between_bb(checksq, ksq));

          SERIALIZE(att, midsq);
          break;
        case CANNON:
          att = pos.attacks_from<CANNON>(midsq) & pos.pieces(~us);

          SERIALIZE(att, midsq);
          natt = pos.attacks_from<ROOK>(midsq) & ~pos.pieces() & (~between_bb(checksq, ksq));

          SERIALIZE(natt, midsq);
          break;
        case KNIGHT:
          att = pos.attacks_from<KNIGHT>(midsq) & ~pos.pieces(us);

          SERIALIZE(att, midsq);
          break;
        case PAWN:
          att = pos.attacks_from<PAWN>(midsq, us) & ~pos.pieces(us) & (~between_bb(checksq, ksq));

          SERIALIZE(att, midsq);
          break;
        case BISHOP:
          att = pos.attacks_from<BISHOP>(midsq) & ~pos.pieces(us);

          SERIALIZE(att, midsq);
          break;
        case ADVISOR:
          att = pos.attacks_from<ADVISOR>(midsq) & ~pos.pieces(us);

          SERIALIZE(att, midsq);
          break;
        case KING:
        default:
          break;
        }
      }

      target = (between_bb(checksq, ksq) & ~occ) | checksq;

      if (target)
      {
        moveList = us == WHITE ? generate_all<WHITE, EVASIONS>(pos, moveList, target, midsq)
          : generate_all<BLACK, EVASIONS>(pos, moveList, target, midsq);
      }

      return moveList;
    }
    else
    {
      switch (pt)
      {
      case ROOK:
        target |= between_bb(checksq, ksq) | checksq;
        break;
      case KNIGHT:
        // Knight leg is checksq leg, which is the same as king's leg
        target |= (KnightLeg[checksq] & KnightEye[ksq]) | checksq;
        break;
      case PAWN:
        target |= checksq;
        break;
      case CANNON:
      default:
        break;
      }

      if (target)
      {
        moveList = us == WHITE ? generate_all<WHITE, EVASIONS>(pos, moveList, target, SQ_NONE)
          : generate_all<BLACK, EVASIONS>(pos, moveList, target, SQ_NONE);
      }
      return moveList;
    }
  }
}

/// generate<LEGAL> generates all the legal moves in the given position

template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList)
{
  ExtMove* cur = moveList;
  Color us = pos.side_to_move();
  Square ksq = pos.square<KING>(us);
  Bitboard pinned = pos.pinned_pieces(us);
  Bitboard cannonsforbid = pos.discovered_cannon_face_king();

  moveList = pos.checkers() ? generate<EVASIONS    >(pos, moveList)
    : generate<NON_EVASIONS>(pos, moveList);
  while (cur != moveList)
  {
    // There are several cases where filtering can significantly improve efficiency
    if (((cannonsforbid & to_sq(cur->move)) && from_sq(cur->move) != ksq)
      || ((pinned & from_sq(cur->move) || from_sq(cur->move) == ksq) && !move_is_legal(pos, cur->move)))
      *cur = (--moveList)->move;
    else
      ++cur;
  }
  return moveList;
}
