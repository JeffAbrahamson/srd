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
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <unistd.h>

#include "leaf.h"
#include "mode.h"
#include "test_text.h"
#include "types.h"


using namespace srd;
using namespace std;



static int test_leaf(string);


int main(int argc, char *argv[])
{
        cout << "Testing leaf.cpp" << endl;
        
        mode(Verbose, false);
        mode(Testing, true);
        
        int err_count = 0;
        vector_string messages = test_text();
        err_count = count_if(messages.begin(), messages.end(), test_leaf);

        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}



/*
  Make a leaf and confirm that re-invoking the leaf gives us the same
  data that we stored.

  For simplicity, we start with a message, which will be the payload,
  and then compute key and password as hashes of the message.
*/
static int test_leaf(const string message)
{
        // Arbitrary assignments
        string key = message_digest(message);
        string password = message_digest(key);

        string base_name, dir_name, full_path;
        {
                leaf first_leaf(password, "", "", true);
                first_leaf.key(key);
                first_leaf.payload(message);
                base_name = first_leaf.basename();
                dir_name = first_leaf.dirname();
                full_path = first_leaf.full_path();
                // and on destruction here, it will be persisted.
        }
        
        leaf second_leaf(password, base_name, dir_name, true);
        int ret = 0;
        if(second_leaf.key() != key) {
                cout << "Key mismatch" << endl;
                ret++;
        } else if(second_leaf.payload() != message) {
                cout << "Payload mismatch" << endl;
                ret++;
        }
        if(second_leaf.full_path() != full_path) {
                cout << "Paths unequal!" << endl;
                cout << "  1=" << full_path << endl;
                cout << "  2=" << second_leaf.full_path() << endl;
                ret++;
        }

        second_leaf.erase();
        leaf third_leaf(password, base_name, dir_name, true);
        if(third_leaf.key() != "" || third_leaf.payload() != "") {
                cout << "Failed to remove leaf." << endl;
                ret++;
        }        
        
        return ret;
}

