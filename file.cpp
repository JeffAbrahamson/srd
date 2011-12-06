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



#include <errno.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "crypt.h"
#include "file.h"
#include "mode.h"


using namespace srd;
using namespace std;


/*
  Create a file if we know something about what to call it.
*/
file::file(const string base_name,
           const string dir_name)
        : m_dir_name(dir_name), m_base_name(base_name), m_dir_verified(false)
{
}



/*
  Create a file if we don't know anything about what to call it.
*/
file::file()
        : m_dir_verified(false)
{
}



/*
  Return the name of the directory in which the file lives or will live.
  Compute the name if needed.  Create the directory if needed.
*/
const string file::dirname() {
        if("" == m_dir_name) {
                // Test mode only affects what directory we generate, if asked.
                if(mode(Testing)) {
                        ostringstream spath;
                        spath << "srd-test-" << getpid();
                        m_dir_name = spath.str();
                } else
                        m_dir_name = string(getenv("HOME")) + "/srd2/";
        }
        if(!m_dir_verified && mkdir(m_dir_name.c_str(), 0700) && EEXIST != errno) {
                cerr << "  Error creating directory \""
                     << m_dir_name << "\": " << strerror(errno) << endl;
                throw(runtime_error("Failed to create directory."));
        }
        m_dir_verified = true;
        return m_dir_name;
}



/*
  Return the name by which we will know the file.
  Compute the name if needed.
*/
const string file::basename() {
        //if("" == m_base_name) {
        if(m_base_name == "") {
                ostringstream sname;
                sname << getpid() << getppid() << time(NULL);
                sname << pseudo_random_string(20);
                m_base_name = message_digest(sname.str(), true);
        }
        return m_base_name;
}


/*
  Set the contents of the file.
  The file need not yet exist.
*/
void file::file_contents(string data)
{
        ofstream fs(full_path().c_str(), ios::out | ios::binary);
        if(!fs.is_open()) {
                char *err_str = strerror(errno);
                ostringstream oss(string("Failed to open file \""));
                oss << full_path() << "\" for writing: " << err_str;
                throw(runtime_error(oss.str()));
        }
        fs.write(data.data(), data.size());
        fs.close();
        
}



/*
  Return the contents of the file.
  It is an error for the file not to exist.
*/
string file::file_contents()
{
        ifstream fs(full_path().c_str(), ios::in | ios::binary | ios::ate);
        if(!fs.is_open()) {
                ostringstream oss(string("Failed to open file \""));
                oss << full_path() << "\" for reading.";
                throw(runtime_error(oss.str()));
        }
        size_t size = fs.tellg();
        if(0 == size)
                return string();
        char *data = new char[size];
        fs.seekg(0, ios::beg);
        fs.read(data, size);
        fs.close();
        string data_str(data, size);
        return data_str;
}



/*
  Remove the underlying file.
  It is not an error later to rewrite the file, and the object remains
  valid after calling rm().
*/
void file::rm()
{
        int rm_ret = unlink(full_path().c_str());
        if(rm_ret) {
                cerr << "  Error removing file:  " << strerror(errno) << endl;
                cerr << "    [File=" << full_path() << "]" << endl;
        }
}

        

/*
  Return true if the file already exists, false otherwise.
*/
bool file::exists()
{
        struct stat buf;
        if(stat(full_path().c_str(), &buf)) {
                int error = errno;
                if(ENOENT == errno)
                        return false;
                throw(runtime_error(strerror(error)));
        }
        return true;
}
