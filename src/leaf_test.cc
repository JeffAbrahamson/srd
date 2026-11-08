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

#include "srd.h"
#include "test_text.h"

using namespace srd;
using namespace std;

namespace {

int test_leaf(string);

/*
  Make a leaf and confirm that re-invoking the leaf gives us the same
  data that we stored.

  For simplicity, we start with a message, which will be the payload,
  and then compute the record's key as a hash of the message.
*/
int test_leaf_with(const string message, const KeyPtr &db_key) {
  // Arbitrary assignments
  string key = message_digest(message);

  string base_name, dir_name, full_path;
  {
    Leaf first_leaf(db_key, "", "");
    first_leaf.key(key);
    first_leaf.payload(message);
    base_name = first_leaf.basename();
    dir_name = first_leaf.dirname();
    full_path = first_leaf.full_path();
    // and on destruction here, it will be persisted.
  }

  Leaf second_leaf(db_key, base_name, dir_name);
  int ret = 0;
  if (second_leaf.key() != key) {
    cout << "Key mismatch" << endl;
    ret++;
  } else if (second_leaf.payload() != message) {
    cout << "Payload mismatch" << endl;
    ret++;
  }
  if (second_leaf.full_path() != full_path) {
    cout << "Paths unequal!" << endl;
    cout << "  1=" << full_path << endl;
    cout << "  2=" << second_leaf.full_path() << endl;
    ret++;
  }

  bool read_only_delete_rejected = false;
  mode(ReadOnly, true);
  try {
    second_leaf.erase();
  } catch (const runtime_error &) {
    read_only_delete_rejected = true;
  }
  mode(ReadOnly, false);
  if (!read_only_delete_rejected || !second_leaf.exists()) {
    cout << "Read-only mode permitted leaf removal." << endl;
    ret++;
  }

  // A format 2 leaf is bound to its name:  a copy under another name
  // doesn't decrypt.
  if (Key::Format2 == db_key->format()) {
    Leaf copy(db_key, base_name + "-copy", dir_name, false);
    copy.file_contents(second_leaf.file_contents());
    try {
      Leaf loaded(db_key, base_name + "-copy", dir_name);
      cout << "A leaf copied to another name still loaded." << endl;
      ret++;
    } catch (const runtime_error &) {
    }
    copy.rm();
  }

  second_leaf.erase();
  if (second_leaf.exists()) {
    cout << "Failed to remove leaf." << endl;
    ret++;
  }
  // A leaf loaded by name must exist, not pass for an empty one.
  try {
    Leaf third_leaf(db_key, base_name, dir_name);
    cout << "A removed leaf loaded." << endl;
    ret++;
  } catch (const runtime_error &) {
  }

  return ret;
}

int test_leaf(const string message) {
  KeyPtr format1_key = make_shared<const Key>(
      Key::Format1, message_digest(message_digest(message)));
  KeyPtr format2_key = make_shared<const Key>(
      Key::Format2, derive_subkey(derive_master_key(message, new_kdf_params()),
                                  EncryptionSubkey));
  return test_leaf_with(message, format1_key) +
         test_leaf_with(message, format2_key);
}
}

int main() {
  cout << "Testing leaf.cpp" << endl;

  mode(Verbose, false);
  mode(Testing, true);
  mode(ReadOnly, false);

  int err_count = 0;
  vector_string messages = test_text();
  err_count = count_if(messages.begin(), messages.end(), test_leaf);

  if (err_count)
    cout << "Errors (" << err_count << ") in test!!" << endl;
  else
    cout << "All tests passed!" << endl;
  return 0 != err_count;
}
