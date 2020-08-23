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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <cstdio>
#include <cstdint>
#include <algorithm>

class Bitboard
{
public:
  Bitboard() { bb[0] = bb[1] = 0; }
  Bitboard(uint64_t low, uint64_t hig) { bb[0] = low & BIT_MASK; bb[1] = hig & BIT_MASK; }

  inline operator bool() const { return bb[0] || bb[1]; }
  inline bool operator == (const Bitboard &board) const { return bb[0] == board.bb[0] && bb[1] == board.bb[1]; }
  inline bool operator != (const Bitboard &board) const { return bb[0] != board.bb[0] || bb[1] != board.bb[1]; }
  inline Bitboard operator ~() const { return Bitboard(~bb[0], ~bb[1]); }
  inline Bitboard operator &(const Bitboard &board) const { return Bitboard(bb[0] & board.bb[0], bb[1] & board.bb[1]); }
  inline Bitboard operator |(const Bitboard &board) const { return Bitboard(bb[0] | board.bb[0], bb[1] | board.bb[1]); }
  inline Bitboard operator ^(const Bitboard &board) const { return Bitboard(bb[0] ^ board.bb[0], bb[1] ^ board.bb[1]); }
  inline Bitboard &operator &=(const Bitboard &board) { bb[0] &= board.bb[0]; bb[1] &= board.bb[1]; return *this; }
  inline Bitboard &operator |=(const Bitboard &board) { bb[0] |= board.bb[0]; bb[1] |= board.bb[1]; return *this; }
  inline Bitboard &operator ^=(const Bitboard &board) { bb[0] ^= board.bb[0]; bb[1] ^= board.bb[1]; return *this; }

  inline Bitboard operator <<(int bit)
  {
    if (bit < 0)
      return *this >> -bit;
    else if (bit <= 45)
      return Bitboard(bb[0] << bit, bb[1] << bit | bb[0] >> (45 - bit));
    else if (bit <= 90)
      return Bitboard(0, bb[0] << (bit - 45));
    else
      return Bitboard();
  }

  inline Bitboard operator >> (int bit)
  {
    if (bit < 0)
      return *this << -bit;
    else if (bit <= 45)
      return Bitboard(bb[0] >> bit | bb[1] << (45 - bit), bb[1] >> bit);
    else if (bit <= 90)
      return Bitboard(bb[1] >> (bit - 45), 0);
    else
      return Bitboard();
  }

  inline Bitboard &operator <<=(int bit)
  {
    if (bit < 0)
    {
      *this >>= -bit;
    }
    else if (bit <= 45)
    {
      bb[1] <<= bit;
      bb[1] |= bb[0] >> (45 - bit);
      bb[0] <<= bit;

      bb[0] &= BIT_MASK;
      bb[1] &= BIT_MASK;
    }
    else if (bit <= 90)
    {
      bb[1] = bb[0] << (bit - 45);
      bb[0] = 0;

      bb[1] &= BIT_MASK;
    }
    else
    {
      bb[0] = 0;
      bb[1] = 0;
    }
    return *this;
  }

  inline Bitboard &operator >>=(int bit)
  {
    if (bit < 0)
    {
      *this <<= -bit;
    }
    else if (bit <= 45)
    {
      bb[0] >>= bit;
      bb[0] |= bb[1] << (45 - bit);
      bb[1] >>= bit;

      bb[0] &= BIT_MASK;
    }
    else if (bit <= 90)
    {
      bb[0] = bb[1] >> (bit - 45);
      bb[1] = 0;
    }
    else
    {
      bb[0] = 0;
      bb[1] = 0;
    }
    return *this;
  }

  inline void pop_lsb()
  {
    if (bb[0])
      bb[0] &= bb[0] - 1;
    else if (bb[1])
      bb[1] &= bb[1] - 1;
  }

  inline bool more_than_one() const
  {
    uint32_t c = 0;
    if (bb[0])
    {
      if (bb[0] & (bb[0] - 1)) return true;
      ++c;
    }

    if (bb[1])
    {
      if (c)  return true;
      if (bb[1] & (bb[1] - 1)) return true;
      ++c;
    }

    return c > 1;
  }

  inline bool equal_to_two() const
  {
    uint32_t c = 0;
    uint64_t t = bb[0];

    while (t) {
      t &= t - 1;
      ++c;
      if (c > 2) return false;
    }

    t = bb[1];
    while (t) {
      t &= t - 1;
      ++c;
      if (c > 2) return false;
    }

    return c == 2;
  }

public:
  uint64_t bb[2];

protected:
  static const uint64_t BIT_MASK = 0x1FFFFFFFFFFF;
};

#endif // #ifndef BITBOARD_H_INCLUDED
