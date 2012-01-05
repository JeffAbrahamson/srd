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
#include <assert.h>
#include <iostream>
#include <pstreams/pstream.h>
#include <string>
#include <vector>

#include "crypt.h"
#include "mode.h"
#include "test_text.h"
#include "types.h"


using namespace srd;
using namespace std;


namespace {
        
        int test_message_digest(const string message);
        int test_encryption(const string message);
        int test_lengths();



        /*
          Return the number of errors that occur.
        */
        int test_message_digest(const string message)
        {
                // I don't believe I will see a bug related to having a
                // single-quote in a string, but the test is easy to write if
                // I assume I don't.  So replace ' with $ in the input string.
                string safe_message;
                replace_copy(message.begin(), message.end(),
                             back_inserter(safe_message), '\'', '$');
        
                string msg_digest = message_digest(safe_message, false);

                string command("echo -n '");
                command.append(safe_message);
                command.append("' | openssl dgst -sha256 -binary | openssl base64 -e");
                redi::ipstream proc(command.c_str());
                string openssl_digest;
                proc >> openssl_digest;

                if(msg_digest == openssl_digest)
                        return 0;   // passed

                cout << "Message digest test failed.  Message=\""
                     << safe_message << "\"" << endl;
                return 1;               // failed
        }



        /*
          Return the number of errors that occur.  Return true if an
          error occurs, false otherwise.
        */
        int test_encryption(const string message)
        {
                int ret = 0;
                string password(message_digest(message, false));
                Crypt cryptor;
                string cipher_text = cryptor.encrypt(message, password);
                string plain_text = cryptor.decrypt(cipher_text, password);
                if(cipher_text == message) {
                        cout << "Encryption did not change message!" << endl;
                        ret++;
                }
                if(message == plain_text)
                        return ret;
                cout << "Decryption did not restore message!" << endl;
                return ret + 1;
        }



        /*
          Check that message_digest() and pseudo_random_string()
          return reasonable values.
        */
        int test_lengths()
        {
                const int N = 5000;
                int prs_error_count = 0;
                int md_error_count = 0;
                int mdfs_error_count = 0;
                srand((int)time(0));
                unsigned int n;
                cout << "  [begin length test]" << endl;
                time_t start_time = time(0);
                assert(start_time > 0);
                for(int i = 0; i < N; i++) {
                        n = rand() & 0xFFFF; // Else we take too long
                        string s(pseudo_random_string(n));
                        if(s.size() != n)
                                prs_error_count++;
                        string md(message_digest(s));
                        if(md.size() != 44)
                                md_error_count++;
                        string mdfs(message_digest(s, true));
                        if(mdfs.size() != 44)
                                mdfs_error_count++;
                        if(mdfs.find('/') != string::npos)
                                mdfs_error_count++;
                }
                time_t end_time = time(0);
                assert(end_time > 0);
                cout << "  ...done in " << end_time - start_time << " seconds." << endl;

                if(prs_error_count)
                        cout << prs_error_count <<
                                " errors from pseudo_random_string()." << endl;
                if(md_error_count)
                        cout << md_error_count <<
                                " errors from message_digest(,false)." << endl;
                if(mdfs_error_count)
                        cout << mdfs_error_count <<
                                " errors from message_digest(,true)." << endl;
                return prs_error_count = md_error_count + mdfs_error_count;
        }        
}


int main(int argc, char *argv[])
{
        cout << "Testing crypt.cpp" << endl;

        mode(Verbose, false);
        mode(Testing, true);

        int err_count = 0;
        vector_string messages = test_text();
        err_count = count_if(messages.begin(), messages.end(), test_message_digest);
        err_count += count_if(messages.begin(), messages.end(), test_encryption);
        err_count += test_lengths();

        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}

