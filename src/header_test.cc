/*
  Copyright 2026  Jeff Abrahamson

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

#include <iostream>
#include <stdexcept>
#include <string>

#include "srd.h"

using namespace srd;
using namespace std;

namespace {

int expect_decode_failure(const string &what, const string &data) {
  try {
    Header::decode(data);
  } catch (const runtime_error &) {
    return 0;
  }
  cout << "Decoded a bad header:  " << what << endl;
  return 1;
}

bool same(const KdfParams &a, const KdfParams &b) {
  return a.opslimit == b.opslimit && a.memlimit == b.memlimit &&
         a.salt == b.salt;
}

/*
  Encoding round-trips, and damage is detected.
*/
int test_encoding() {
  int errors = 0;
  KdfParams params = new_kdf_params();
  params.opslimit = 0x01020304;
  params.memlimit = 0x0102030405060708ULL;
  string data = Header::encode(params);
  if (!same(params, Header::decode(data))) {
    cout << "Header didn't round-trip." << endl;
    errors++;
  }
  errors += expect_decode_failure("empty", "");
  errors += expect_decode_failure("short", data.substr(0, data.size() - 1));
  errors += expect_decode_failure("long", data + "x");
  string bad = data;
  bad[0] = 'X';
  errors += expect_decode_failure("magic", bad);
  bad = data;
  bad[4] = 3;
  errors += expect_decode_failure("version", bad);
  bad = data;
  bad[5] = 1;
  errors += expect_decode_failure("algorithm", bad);
  return errors;
}

/*
  The header is created once and then reused.
*/
int test_file() {
  int errors = 0;
  Header header("");
  if (header.exists()) {
    cout << "Test directory already has a header." << endl;
    return 1;
  }
  KdfParams created = header.params_or_create();
  if (!header.exists() || !same(created, header.params()) ||
      !same(created, Header(header.dirname()).params_or_create())) {
    cout << "Header wasn't created once and reused." << endl;
    errors++;
  }
  // Outside test mode, the test costs are refused.
  mode(Testing, false);
  try {
    header.params();
    cout << "Test-cost header accepted outside test mode." << endl;
    errors++;
  } catch (const runtime_error &) {
  }
  mode(Testing, true);
  header.rm();
  return errors;
}
}

int main() {
  cout << "Testing header.cc" << endl;

  mode(Verbose, false);
  mode(Testing, true);
  mode(ReadOnly, false);

  int err_count = test_encoding() + test_file();

  if (err_count)
    cout << "Errors (" << err_count << ") in test!!" << endl;
  else
    cout << "All tests passed!" << endl;
  return 0 != err_count;
}
