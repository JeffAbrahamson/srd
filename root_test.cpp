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


#include <boost/bind.hpp>
#include <errno.h>
#include <string>
#include <string.h>
#include <sstream>
#include <unistd.h>

#include "crypt.h"
#include "mode.h"
#include "root.h"
#include "test_text.h"
#include "types.h"


using namespace srd;
using namespace std;



static int test_root_basic(string password);
static int test_root_singles();
//static void add_pair(root &root, pair<string, string> couple);
int confirm_once(root &root, pair<string, string> text);



int main(int argc, char *argv[])
{
        cout << "Testing root.cpp" << endl;
        
        mode(Verbose, false);
        mode(Testing, true);
        string password = pseudo_random_string(20);
        
        int err_count = test_root_basic(password);
        err_count += test_root_basic(password);
        err_count += test_root_singles();
        
        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}



/*
  Message key is hash of message.
  Password is hash of key.
  Persist the root node, then reconstitute it.
  Confirm that we get back what we expect.
*/
static int test_root_basic(string password)
{
        int error_count = 0;
        vector_string messages = test_text();

        {
                root root(password, "");

                for(vector_string::iterator it = messages.begin();
                    it != messages.end();
                    it++) {
                        ostringstream ss;
                        ss << it->size();
                        string key(ss.str());
                        string foo = *it;
                        string payload(*it);
                        root.add_leaf(key, payload);
                }
        }
        
        cout << "Re-instantiating root." << endl;
        {
                root root(password, "");
                for(vector_string::iterator it = messages.begin();
                    it != messages.end();
                    it++) {
                        // For each message, confirm that we can find it.
                        ostringstream ss;
                        ss << it->size();
                        string key(ss.str());
                        string foo = *it;
                        string payload(*it);
                        // So we are expecting (key, payload)
                        vector_string payloads_to_find;
                        payloads_to_find.push_back(*it);
                        leaf_proxy_map results = root.filter_payloads(payloads_to_find);
                        if(0 == results.size())
                                error_count++;
                }
        }
        return error_count;
}



/*
  Exercise root with distinct keys and known messages.
*/
static int test_root_singles()
{
        int error_count = 0;
        map<string, string> text = orderly_text();
        // A new password to guarantee a new root.
        // But mode(Testing) is still true, so we'll nonetheless
        // use the testing directory.  It will just have an additional
        // root (and leaves) stored in it.
        string password = pseudo_random_string(15);
        {
                // Instantiate and add (key,value) pairs
                root root(password, "");
                // Using a std::for_each and boost::bind here would add to a
                // temporary object, so iterate by hand.  Is there a better way?
                for(map<string, string>::const_iterator it = text.begin();
                    it != text.end();
                    it++) {
                        root.add_leaf(it->first, it->second);
                }
                // Read-only, but that's fine.  Probably making an unnecessary copy.
                int errors = count_if(text.begin(),
                                      text.end(),
                                      boost::bind(&confirm_once, root, _1));
                error_count += errors;
        }
        
        return error_count;
}



/*
  Check that each (key, value) pair in text appears precisely once in root.
  Return the number of errors found, so 0 assuming all is well.
*/
int confirm_once(root &root, pair<string, string> text)
{
        int ret = 0;
        vector_string key_pattern;
        key_pattern.push_back(text.first);
        leaf_proxy_map key_results = root.filter_keys(key_pattern);
        if(key_results.size() != 1) {
                cout << "Key \"" << text.first << ":  "
                     << "Found " << key_results.size() << " keys expected 1."
                     << endl;
                ret++;
        }

        vector_string payload_pattern;
        payload_pattern.push_back(text.second);
        leaf_proxy_map payload_results = root.filter_payloads(payload_pattern);
        if(payload_results.size() != 1) {
                cout << "Payload \"" << text.second << "\":  "
                     << "Found " << payload_results.size() << " leaves, expected 1."
                     << endl;
                ret++;
        }

        payload_pattern.clear();
        payload_pattern.push_back(text.second.substr(3,8));
        payload_results = root.filter_keys(key_pattern).filter_payloads(payload_pattern);
        if(payload_results.size() != 1) {
                cout << "Payload \"" << text.second << "(3,8)\":  "
                     << "Found " << payload_results.size() << " leaves, expected 1."
                     << endl;
                ret++;
        }
        
        return ret;
}

