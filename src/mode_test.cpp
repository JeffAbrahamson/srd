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

#include "mode.h"


//using namespace srd;
using namespace std;


int check_mode(Mode m, bool b);


/*
  Mode is awfully simple...
*/
int main(int argc, char *argv[])
{
        cout << "Testing mode.cpp" << endl;

        int error_count = 0;

        mode(Verbose, false);
        error_count += check_mode(Verbose, false);
        mode(Verbose, true);
        error_count += check_mode(Verbose, true);
        mode(Verbose, false);
        error_count += check_mode(Verbose, false);

        mode(Testing, false);
        error_count += check_mode(Testing, false);
        mode(Testing, true);
        error_count += check_mode(Testing, true);
        mode(Testing, false);
        error_count += check_mode(Testing, false);
        mode(Testing, true);
        mode(Testing, true);
        mode(Testing, true);
        error_count += check_mode(Testing, true);
                
        error_count += check_mode(Verbose, false);
        mode(Verbose, false);
        error_count += check_mode(Verbose, false);
        mode(Verbose, true);
        error_count += check_mode(Verbose, true);
        mode(Verbose, false);
        error_count += check_mode(Verbose, false);
}



int check_mode(Mode m, bool b)
{
        if(mode(m) == b)
                return 0;
        return 1;
}
