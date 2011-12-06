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


static int test_message_digest(const string message);
static int test_encryption(const string message);



int main(int argc, char *argv[])
{
        cout << "Testing crypt.cpp" << endl;
        
        mode(Verbose, false);
        mode(Testing, true);
        
        int err_count = 0;
        vector_string messages = test_text();
        err_count = count_if(messages.begin(), messages.end(), test_message_digest);
        err_count += count_if(messages.begin(), messages.end(), test_encryption);

        if(err_count)
                cout << "Errors (" << err_count << ") in test!!" << endl;
        else
                cout << "All tests passed!" << endl;
        return 0 != err_count;
}


/*
  Return the number of errors that occur.
*/
static int test_message_digest(const string message)
{
        // I don't believe I will see a bug related to having a
        // single-quote in a string, but the test is easy to write if
        // I assume I don't.  So replace ' with $ in the input string.
        string safe_message;
        replace_copy(message.begin(), message.end(), back_inserter(safe_message), '\'', '$');
        
        string msg_digest = message_digest(safe_message, false);

        string command("echo -n '");
        command.append(safe_message);
        command.append("' | openssl dgst -sha256 -binary | openssl base64 -e");
        redi::ipstream proc(command.c_str());
        string openssl_digest;
        proc >> openssl_digest;

        if(msg_digest == openssl_digest)
                return 0;   // passed

        cout << "Message digest test failed.  Message=\"" << safe_message << "\"" << endl;
        return 1;               // failed
}



/*
  Return the number of errors that occur.  Return true if an error occurs, false otherwise.
*/
static int test_encryption(const string message)
{
        int ret = 0;
        string password(message_digest(message, false));
        srd::crypt cryptor;
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




