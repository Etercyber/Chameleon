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

#ifndef BOOK_H_INCLUDED
#define BOOK_H_INCLUDED
#define NOMINMAX

#include <fstream>
#include <string>

#include "position.h"
#include "rkiss.h"
#include "misc.h"

class PolyglotBook : private std::ifstream
{
public:
  PolyglotBook() : rkiss(now()) {};
  ~PolyglotBook()
  {
    if (is_open()) close();
  };
  Move probe(const Position &pos, const std::string &fName, bool pickBest);

private:
  template<typename T> PolyglotBook &operator>>(T &n);

  bool open(const char *fName);
  size_t find_first(Key key);

  RKISS rkiss;
  std::string fileName;
};

#endif // #ifndef BOOK_H_INCLUDED
