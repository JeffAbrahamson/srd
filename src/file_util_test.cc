/*
  Copyright 2012  Jeff Abrahamson
  
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
#include <unistd.h>
#include <sstream>
#include <sys/types.h>

#include "srd.h"


using namespace srd;
using namespace std;



namespace {


}



int main(int argc, char *argv[])
{
    cout << "Testing file_util.cpp" << endl;
        
    mode(Verbose, false);
    mode(Testing, true);
        
    int err_count = 0;

    ostringstream tmp_name_s;
    tmp_name_s << "/tmp/srd-" << getpid() << "-" << time(0) << "XXXXXX";
    string tmp_name = tmp_name_s.str();
        
    return 0 != err_count;
}
