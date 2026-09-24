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
int test_compress(const string message) {
  int ret = 0;
  string compressed = compress(message);
  string decompressed = decompress(compressed);
  if (compressed == message) {
    cout << "Compress did not change the message!" << endl;
    ret++;
  }
  if (decompressed != message) {
    cout << "decompress() failed to restore the message!" << endl;
    ret++;
  }
  if (message.size() > 100 && compressed.size() > message.size()) {
    // Ignore the cases where small messages expand
    cout << "Message size change: " << message.size()
         << " <= " << compressed.size() << endl;
    ret++;
  }
  return ret;
}

/*
  Large records used to overflow the stack.  Highly compressible data
  was worst, since decompression guessed 5x the compressed size and
  retried recursively, doubling each time.
  Return number of errors that occur.
*/
int test_large() {
  int ret = 0;
  string compressible(50 * 1000 * 1000, 'a');
  if (decompress(compress(compressible)) != compressible) {
    cout << "Failed to round-trip a large compressible message." << endl;
    ret++;
  }
  string incompressible = pseudo_random_string(10 * 1000 * 1000);
  if (decompress(compress(incompressible)) != incompressible) {
    cout << "Failed to round-trip a large incompressible message." << endl;
    ret++;
  }
  if (!decompress(compress(string())).empty()) {
    cout << "Failed to round-trip an empty message." << endl;
    ret++;
  }
  return ret;
}

/*
  Corrupt input must throw domain_error, which callers treat as a
  damaged record.
  Return number of errors that occur.
*/
int test_corrupt() {
  int ret = 0;
  string compressed = compress(test_text()[0]);
  vector<pair<string, string>> cases = {
      {"truncated", compressed.substr(0, compressed.size() / 2)},
      {"garbage", string("not bzip2 data at all")},
      {"empty", string()}};
  for (const auto &[name, data] : cases) {
    try {
      decompress(data);
      cout << "Decompressing " << name << " data did not throw." << endl;
      ret++;
    } catch (const domain_error &) {
    }
  }
  return ret;
}

}  // namespace

int main() {
  cout << "Testing compress.cpp" << endl;

  mode(Verbose, false);
  mode(Testing, true);

  int err_count = 0;
  vector_string messages = test_text();
  err_count = count_if(messages.begin(), messages.end(), test_compress);
  err_count += test_large();
  err_count += test_corrupt();

  if (err_count)
    cout << "Errors (" << err_count << ") in test!!" << endl;
  else
    cout << "All tests passed!" << endl;
  return 0 != err_count;
}
