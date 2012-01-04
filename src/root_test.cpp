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
#include <time.h>
#include <unistd.h>

#include "crypt.h"
#include "mode.h"
#include "root.h"
#include "test_text.h"
#include "types.h"


using namespace srd;
using namespace std;


namespace {

        int test_root_basic(string password);
        int test_root_singles();
        int test_root_change_password();
        int test_ordering();
        bool confirm_ordering(root &root);
        //void add_pair(root &root, pair<string, string> couple);
        int confirm_once(root &root, pair<string, string> text);


        /*
          Message key is hash of message.
          Password is hash of key.
          Persist the root node, then reconstitute it.
          Confirm that we get back what we expect.
        */
        int test_root_basic(string password)
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
        int test_root_singles()
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
                        root root(password, "", true);
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
                leaf_proxy_map key_results = root.filter_keys(key_pattern, false);
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
                payload_results = root.filter_keys(key_pattern, false).filter_payloads(payload_pattern);
                if(payload_results.size() != 1) {
                        cout << "Payload \"" << text.second << "(3,8)\":  "
                             << "Found " << payload_results.size() << " leaves, expected 1."
                             << endl;
                        ret++;
                }
        
                return ret;
        }



        /*
          Add random data to the root.
          Confirm that the ordering is correct (based on key).
          Persist the root, reload, and confirm again.
        */
        int test_ordering()
        {
                int N = 5000;
                string password = pseudo_random_string(15);
                {
                        cout << "Creating " << N << " leaves..." << endl;
                        root root(password, "", true);
                        time_t start_time = time(0);
                        assert(start_time > 0);
                        for(int i = 0; i < N; i++) {
                                string key = message_digest(pseudo_random_string(30));
                                string payload = message_digest(pseudo_random_string(100));
                                root.add_leaf(key, payload);
                        }
                        if(!confirm_ordering(root)) {
                                cout << "Root order test failed before persist." << endl;
                                return 1;
                        }
                        time_t end_time = time(0);
                        assert(end_time > 0);
                        cout << "  ...done in " << end_time - start_time << " seconds." << endl;
                }
                {
                        cout << "Verifying " << N << " leaves." << endl;
                        root root(password, "");
                        time_t start_time = time(0);
                        assert(start_time > 0);
                        confirm_ordering(root);
                        if(!confirm_ordering(root)) {
                                cout << "Root order test failed after persist." << endl;
                                return 1;
                        }
                        time_t end_time = time(0);
                        assert(end_time > 0);
                        cout << "  ...done in " << end_time - start_time << " seconds." << endl;
                }
                return 0;
        }



        /*
          Return true if leaves are ordered.
          Return false if not.
        */
        bool confirm_ordering(root &root)
        {
                int errors = 0;
                string last_key;        // any key is >= ""
                leaf_proxy_map::LPM_Set leaves = root.as_set();
                assert(leaves.size() == root.size());
                for(leaf_proxy_map::LPM_Set::iterator it = leaves.begin();
                    it != leaves.end();
                    it++) {
                        // std::set iterators are const, since we can't modify
                        // set elements.
                        const string key((*it).key());
                        if(key < last_key)
                                errors++;
                        last_key = key;
                }
                if(errors > 0)
                        cout << "Found " << errors << " ordering errors." << endl;
                return 0 == errors;
        }



        /*
          Instantiate a root, add some leaves, change the password, and see if
          we get the same data back.
        */
        int test_root_change_password()
        {
                int error_count = 0;
                vector_string messages = test_text();
                string password = pseudo_random_string(20);
                string password2 = pseudo_random_string(20);

                {
                        root root(password, "", true);

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

                cout << "Re-instantiating root and changing password." << endl;
                {
                        root old_root(password, "");
                        root new_root = old_root.change_password(password2);

                        cout << "Checking the new root." << endl;
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
                                leaf_proxy_map results = new_root.filter_payloads(payloads_to_find);
                                if(0 == results.size())
                                        error_count++;
                        }
                }
                return error_count;
        }
}



int main(int argc, char *argv[])
{
        cout << "Testing root.cpp" << endl;
        
        mode(Verbose, false);
        mode(Testing, true);
        string password = pseudo_random_string(20);

        { root(password, "", true); } // Create this root
        int err_count = test_root_basic(password);
        err_count += test_root_basic(password);
        err_count += test_root_singles();
        err_count += test_root_change_password();
        err_count += test_ordering();
        
        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}

