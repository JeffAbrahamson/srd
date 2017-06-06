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

#include "srd.h"
#include "test_text.h"


using namespace srd;
using namespace std;


namespace {
        
    int test_compress(string message);

    /*
      Return number of errors that occur.
    */
    int test_compress(const string message)
    {
	int ret = 0;
	string compressed = compress(message);
	string decompressed = decompress(compressed);
	if(compressed == message) {
	    cout << "Compress did not change the message!" << endl;
	    ret++;
	}
	if(decompressed != message) {
	    cout << "decompress() failed to restore the message!" << endl;
	    ret++;
	}                
	if(message.size() > 100 && compressed.size() > message.size()) {
	    // Ignore the cases where small messages expand
	    cout << "Message size change: " << message.size()
		 << " <= " << compressed.size() << endl;
	    ret++;
	}
	return ret;
    }


}


int main(int argc, char *argv[])
{
    cout << "Testing compress.cpp" << endl;

    mode(Verbose, false);
    mode(Testing, true);
        
    int err_count = 0;
    vector_string messages = test_text();
    err_count = count_if(messages.begin(), messages.end(), test_compress);
        
    if(err_count)
	cout << "Errors (" << err_count << ") in test!!" << endl;
    else
	cout << "All tests passed!" << endl;
    return 0 != err_count;
}
