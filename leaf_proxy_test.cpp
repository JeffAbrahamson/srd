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
#include <string>
#include <string.h>
#include <sstream>
#include <unistd.h>

#include "leaf_proxy.h"
#include "mode.h"
#include "test_text.h"
#include "types.h"


using namespace srd;
using namespace std;



static int test_leaf_proxy(string);


int main(int argc, char *argv[])
{
        cout << "Testing leaf_proxy.cpp" << endl;

        mode(Verbose, false);
        mode(Testing, true);
        
        int err_count = 0;
        vector_string messages = test_text();
        err_count = count_if(messages.begin(), messages.end(), test_leaf_proxy);

        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}



/*
  Message key is hash of message.
  Password is hash of key.
  Persist the leaf proxy (so the underlying leaf), then reconstitute it.
*/
static int test_leaf_proxy(const string message)
{
        // Arbitrary assignments
        string key = message_digest(message);
        string password = message_digest(key);

        string base_name, dir_name, full_path;
        {
                leaf_proxy first_leaf_proxy(password, "", "");
                first_leaf_proxy.set(key, message);
                base_name = first_leaf_proxy.basename();
                first_leaf_proxy.commit();
        }
        
        leaf_proxy second_leaf_proxy(password, base_name, "");
        int ret = 0;
        if(second_leaf_proxy.basename().size() == 0) {
                cout << "Proxy has empty basename." << endl;
                ret++;
        }
        if(second_leaf_proxy.key() != key) {
                cout << "Key mismatch" << endl;
                ret++;
        };
        if(second_leaf_proxy.payload() != message) {
                cout << "Payload mismatch" << endl;
                ret++;
        }
        second_leaf_proxy.erase();

        leaf_proxy third_leaf_proxy(password, base_name, "");
        if(third_leaf_proxy.basename().size() == 0) {
                cout << "Proxy has empty basename." << endl;
                ret++;
        }
        if(third_leaf_proxy.key() != "") {
                cout << "Key should be empty after delete, but isn't." << endl;
                ret++;
        };
        if(third_leaf_proxy.payload() != "") {
                cout << "Payload should be empty after delete, but isn't." << endl;
                ret++;
        };

        return ret;
}

