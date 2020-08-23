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

#include <fstream>
#include <iostream>
#include <istream>
#include <vector>

#include "misc.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "uci.h"

using namespace std;

namespace {

  const vector<string> Defaults = {
    "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w",
    "2bak4/9/3a5/p2Np3p/3n1P3/3pc3P/P4r1c1/B2CC2R1/4A4/3AK1B2 b",
    "1r1akabr1/1c7/2n1b1n2/p1p1p3p/6p2/PN3R3/1cP1P1P1P/2C1C1N2/1R7/2BAKAB2 b",
    "2b1ka2r/3na2c1/4b3n/8R/8C/4C1P2/P1P1P3P/4B1N2/1r2A4/2BAK4 w",
    "2bckab2/4a4/5n3/CR3N2p/5r3/P3P1B2/9/2n1B4/4A4/3AK1C2 w",
    "2b1kab1C/1N2a4/n3ccn2/p5r1p/4p4/P1P2RN2/2r1P3P/C3B4/4A4/2BAK2R1 w",
    "2bakab2/9/2n1c1R1c/3r4p/4N4/r8/6P1P/6C1C/4A4/1RBAK1B2 w",
    "2bak1b1r/4a4/2n4cn/p6C1/4pN3/P2N4R/4P1P1P/3CB4/4A2r1/c1BAKR3 w",
    "1r2kabr1/4a4/2C1b2c1/p3p3p/1c3n3/2p3R2/P3P3P/N3C1N2/7R1/2BAKAB2 b",
    "3ak1b2/4a4/2n1b1R2/p1N1pc2p/7r1/2PN1r3/P3P3P/3RB4/4A4/1C2KAB1c w",
    "2baka1r1/9/c5n1c/p3p1CCp/2p3p2/4P4/P6RP/2r1B1N2/4A4/1RB1KA3 w",
    "3akabr1/9/4c4/p1pRn2Cp/4rcp2/2P1p4/P3P1P1P/3CB1N2/9/3AKABR1 w",
    "3akab2/3r5/8n/8p/2P1C1b2/8P/cR2N2r1/2n1B1N2/4A4/2B1KR3 w",
    "2bak4/4a1R2/2n1ccn1b/p3p1C1p/9/2p3P2/P1r1P3P/2N1BCN2/4A4/2BAK4 w",
    "C3kab2/4a4/2Rnb3n/8p/6p2/1p2c3r/P5P2/4B3N/3CA4/2BAK4 w",
    "4kabr1/4a4/2n1b3n/p1C1p3p/6p2/PNP6/4P1P2/1C2B4/4A4/1R2KAB1c w"
  };

} // namespace

/// benchmark() runs a simple benchmark by letting Stockfish analyze a set
/// of positions for a given limit each. There are five parameters: the
/// transposition table size, the number of search threads that should
/// be used, the limit value spent for each position (optional, default is
/// depth 13), an optional file name where to look for positions in FEN
/// format (defaults are the positions defined above) and the type of the
/// limit value: depth (default), time in millisecs or number of nodes.

void benchmark(const Position& current, istream& is) {

  string token;
  vector<string> fens;
  Search::LimitsType limits;

  // Assign default values to missing arguments
  string ttSize = (is >> token) ? token : "16";
  string threads = (is >> token) ? token : "1";
  string limit = (is >> token) ? token : "13";
  string fenFile = (is >> token) ? token : "default";
  string limitType = (is >> token) ? token : "depth";

  Options["Hash"] = ttSize;
  Options["Threads"] = threads;
  Search::clear();

  if (limitType == "time")
    limits.movetime = stoi(limit); // movetime is in millisecs

  else if (limitType == "nodes")
    limits.nodes = stoi(limit);

  else if (limitType == "mate")
    limits.mate = stoi(limit);

  else
    limits.depth = stoi(limit);

  if (fenFile == "default")
    fens = Defaults;

  else if (fenFile == "current")
    fens.push_back(current.fen());

  else
  {
    string fen;
    ifstream file(fenFile);

    if (!file.is_open())
    {
      cerr << "Unable to open file " << fenFile << endl;
      return;
    }

    while (getline(file, fen))
      if (!fen.empty())
        fens.push_back(fen);

    file.close();
  }

  Key nodes = 0;
  TimePoint elapsed = now();

  for (size_t i = 0; i < fens.size(); ++i)
  {
    Position pos(fens[i], Threads.main());

    cerr << "\nPosition: " << i + 1 << '/' << fens.size() << endl;

    if (limitType == "perft")
      nodes += Search::perft(pos, limits.depth * ONE_PLY);

    else
    {
      Search::StateStackPtr st;
      limits.startTime = now();
      Threads.start_thinking(pos, limits, st);
      Threads.main()->wait_for_search_finished();
      nodes += Threads.nodes_searched();
    }
  }

  elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

  dbg_print(); // Just before to exit

  cerr << "\n==========================="
    << "\nTotal time (ms) : " << elapsed
    << "\nNodes searched  : " << nodes
    << "\nNodes/second    : " << 1000 * nodes / elapsed << endl;
}
