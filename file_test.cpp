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


#include <algorithm>
#include <errno.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string.h>
#include <vector>

#include "crypt.h"
#include "file.h"
#include "test_text.h"
#include "types.h"


using namespace srd;
using namespace std;



static int test_exists(const char *dir, const char *base, bool expect_exists);
static int test_file(string message);


int main(int argc, char *argv[])
{
        cout << "Testing file.cpp" << endl;
        
        int err_count = 0;
        err_count += test_exists("/", "tmp", true);
        err_count += test_exists("/", "TMP", false);
        err_count += test_exists("/bin", "echo", true);        
        err_count += test_exists("/bin", "echo-not", false);        

        vector_string messages = test_text();
        err_count = count_if(messages.begin(), messages.end(), test_file);

        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}



static int test_exists(const char *dir, const char *base, bool expect_exists)
{
        file file(string(dir), string(base), true);
        try {
                if(expect_exists == file.exists())
                        return 0;
                return 1;
        }
        catch(exception e) {
                cerr << dir << "/" << base << ":  " << e.what() << endl;
                return 1;
        }
        cerr << dir << "/" << base << " (existence check failed)" << endl;
        return 1;
}



static int test_file(string message)
{
        int ret = 0;
        string filename;   // Remember so we can clean up no matter what
        try {
                file my_file(true);
                filename = my_file.full_path();
                my_file.file_contents(message);
                string dup_message(my_file.file_contents());

                if(message != dup_message) {
                        cout << "File write + read not identity!" << endl;
                        ret++;
                }
        }
        catch(runtime_error e) {
                cerr << e.what() << endl;
                ret++;
        }
        catch(...) {
                cerr << "Something unexpected went wrong." << endl;
                ret++;
        }
        int rm_ret = unlink(filename.c_str());
        if(rm_ret) {
                cerr << "  Error removing test file:  " << strerror(errno) << endl;
                cerr << "    (File=" << filename << ")" << endl;
                ret++;
        }
        
        return ret;
}
