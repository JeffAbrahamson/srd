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


#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "srd.h"



using namespace srd;
using namespace std;




/*
  Create an empty file.
  It is an error if the file already exists.
*/
void srd::file_create(const string &fn)
{
        int fd = creat(fn.c_str(), 0600);
        if(-1 == fd)
                throw(runtime_error(string("Failed to create ") + fn + " :  " + strerror(errno)));
        close(fd);
}



/*
  Return true if file exists, false otherwise.
  Throw a hopefully descriptive error on error.
*/
bool srd::file_exists(const string &fn)
{
        struct stat buf;
        if(stat(fn.c_str(), &buf)) {
                int error = errno;
                if(ENOENT == errno)
                        return false;
                throw(runtime_error(strerror(error)));
        }
        return true;
}


/*
  Remove a file.
*/
void srd::file_rm(const string &fn)
{
        int rm_ret = unlink(fn.c_str());
        if(rm_ret) {
                cerr << "  Error removing file:  " << strerror(errno) << endl;
                cerr << "    [File=" << fn << "]" << endl;
        }
}


