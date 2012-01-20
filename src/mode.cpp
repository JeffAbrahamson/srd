/*
  Copyright 2011  Jeff Abrahamson
  
  This file is part of srd.
  
  srd is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  srd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with srd.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <iostream>
#include <map>

#include "srd.h"


using namespace srd;
using namespace std;


namespace srd {
        static map<Mode, bool> modes;
}


void srd::mode(const Mode m, const bool new_state)
{
        modes[m] = new_state;
}


const bool srd::mode(const Mode m)
{
        map<Mode, bool>::const_iterator it = modes.find(m);
        if(it == modes.end()) {
                cerr << "Failed to find mode " << m << endl;
                cerr << "This is a bug whose effects are undefined." << endl;
                cerr << "Continuing as though the result had been false." << endl;
                return false;
        }
        return it->second;
}


