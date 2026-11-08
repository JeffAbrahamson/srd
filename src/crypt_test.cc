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

#include "srd.h"
#include "test_text.h"

using namespace srd;
using namespace std;

namespace {

int test_message_digest(const string message);
int test_encryption(const string message);
int test_lengths();

/*
  Return the number of errors that occur.
*/
int test_message_digest(const string message) {
  // I don't believe I will see a bug related to having a
  // single-quote in a string, but the test is easy to write if
  // I assume I don't.  So replace ' with $ in the input string.
  string safe_message;
  replace_copy(message.begin(), message.end(), back_inserter(safe_message),
               '\'', '$');

  string msg_digest = message_digest(safe_message, false);

  string command("echo -n '");
  command.append(safe_message);
  command.append("' | openssl dgst -sha256 -binary | openssl base64 -e");
  redi::ipstream proc(command.c_str());
  string openssl_digest;
  proc >> openssl_digest;

  if (msg_digest == openssl_digest)
    return 0; // passed

  cout << "Message digest test failed.  Message=\"" << safe_message << "\""
       << endl;
  return 1; // failed
}

/*
  Return the number of errors that occur.  Return true if an
  error occurs, false otherwise.
*/
int test_encryption(const string message) {
  int ret = 0;
  string password(message_digest(message, false));
  string cipher_text = encrypt(message, password);
  string plain_text = decrypt(cipher_text, password);
  if (cipher_text == message) {
    cout << "Encryption did not change message!" << endl;
    ret++;
  }
  if (message == plain_text)
    return ret;
  cout << "Decryption did not restore message!" << endl;
  return ret + 1;
}

/*
  Check that message_digest() and pseudo_random_string()
  return reasonable values.
*/
int test_lengths() {
  const int N = 5000;
  int prs_error_count = 0;
  int md_error_count = 0;
  int mdfs_error_count = 0;
  srand((int)time(0));
  unsigned int n;
  cout << "  [begin length test]" << endl;
  time_t start_time = time(0);
  assert(start_time > 0);
  for (int i = 0; i < N; i++) {
    n = rand() & 0xFFFF; // Else we take too long
    string s(pseudo_random_string(n));
    if (s.size() != n)
      prs_error_count++;
    string md(message_digest(s));
    if (md.size() != 44)
      md_error_count++;
    string mdfs(message_digest(s, true));
    if (mdfs.size() != 44)
      mdfs_error_count++;
    if (mdfs.find('/') != string::npos)
      mdfs_error_count++;
  }
  time_t end_time = time(0);
  assert(end_time > 0);
  cout << "  ...done in " << end_time - start_time << " seconds." << endl;

  if (prs_error_count)
    cout << prs_error_count << " errors from pseudo_random_string()." << endl;
  if (md_error_count)
    cout << md_error_count << " errors from message_digest(,false)." << endl;
  if (mdfs_error_count)
    cout << mdfs_error_count << " errors from message_digest(,true)." << endl;
  return prs_error_count = md_error_count + mdfs_error_count;
}

/*
  Return 1 if f() doesn't throw runtime_error, else 0.
*/
template <typename F> int expect_throw(const string &what, F f) {
  try {
    f();
  } catch (const runtime_error &) {
    return 0;
  }
  cout << "Expected an exception:  " << what << endl;
  return 1;
}

/*
  Check key derivation:  deterministic, sensitive to passphrase and
  salt, and guarded against weak or costly parameters.
*/
int test_kdf() {
  int errors = 0;
  KdfParams params = new_kdf_params();
  KdfParams other_salt = new_kdf_params();
  if (params.salt == other_salt.salt) {
    cout << "Two new salts are equal." << endl;
    errors++;
  }
  string key = derive_master_key("passphrase", params);
  if (key.size() != 32) {
    cout << "Master key has size " << key.size() << endl;
    errors++;
  }
  if (key != derive_master_key("passphrase", params)) {
    cout << "Key derivation isn't deterministic." << endl;
    errors++;
  }
  if (key == derive_master_key("passphrasf", params) ||
      key == derive_master_key("passphrase", other_salt)) {
    cout << "Key derivation ignores passphrase or salt." << endl;
    errors++;
  }

  KdfParams bad = params;
  bad.salt.pop_back();
  errors += expect_throw("short salt", [&] { check_kdf_params(bad); });
  bad = params;
  bad.memlimit = 8ULL << 30;
  errors += expect_throw("huge memlimit", [&] { check_kdf_params(bad); });
  bad = params;
  bad.opslimit = 1000;
  errors += expect_throw("huge opslimit", [&] { check_kdf_params(bad); });

  // Outside test mode, the minimum (test) costs are refused.
  mode(Testing, false);
  errors += expect_throw("test costs outside test mode",
                         [&] { check_kdf_params(params); });
  KdfParams real = new_kdf_params();
  check_kdf_params(real);
  mode(Testing, true);
  return errors;
}

/*
  Check that subkeys and keyed names are deterministic and distinct.
*/
int test_subkeys() {
  int errors = 0;
  string master = derive_master_key("passphrase", new_kdf_params());
  string enc = derive_subkey(master, EncryptionSubkey);
  string name = derive_subkey(master, RootNameSubkey);
  if (enc.size() != 32 || name.size() != 32 || enc == name ||
      enc == master || enc != derive_subkey(master, EncryptionSubkey)) {
    cout << "Subkeys are wrong." << endl;
    errors++;
  }
  string root_name = keyed_name(name, "root");
  if (root_name.size() != 64 ||
      root_name.find_first_not_of("0123456789abcdef") != string::npos ||
      root_name != keyed_name(name, "root") ||
      root_name == keyed_name(enc, "root") ||
      root_name == keyed_name(name, "roof")) {
    cout << "Keyed names are wrong:  " << root_name << endl;
    errors++;
  }
  errors += expect_throw("short master key",
                         [&] { derive_subkey("short", EncryptionSubkey); });
  return errors;
}

/*
  Check that authenticated encryption round-trips, uses a fresh nonce,
  and rejects any change to the file, the key, or the context.
*/
int test_aead(const string message) {
  int errors = 0;
  string key = derive_subkey(derive_master_key("pw", new_kdf_params()),
                             EncryptionSubkey);
  string cipher_text = aead_encrypt(message, key, "leaf:name");
  if (aead_decrypt(cipher_text, key, "leaf:name") != message) {
    cout << "AEAD round trip failed." << endl;
    errors++;
  }
  if (cipher_text == aead_encrypt(message, key, "leaf:name")) {
    cout << "AEAD reused a nonce." << endl;
    errors++;
  }
  if (cipher_text.find(message) != string::npos && message.size() > 8) {
    cout << "AEAD left the message in the clear." << endl;
    errors++;
  }
  errors += expect_throw("wrong context", [&] {
    aead_decrypt(cipher_text, key, "leaf:other");
  });
  string other_key(key);
  other_key[0] ^= 1;
  errors += expect_throw("wrong key", [&] {
    aead_decrypt(cipher_text, other_key, "leaf:name");
  });
  errors += expect_throw("truncated", [&] {
    aead_decrypt(cipher_text.substr(0, cipher_text.size() - 1), key,
                 "leaf:name");
  });
  errors += expect_throw("empty", [&] { aead_decrypt("", key, "leaf:name"); });
  // Flipping any single byte must be detected.
  for (size_t i = 0; i < cipher_text.size(); i++) {
    string damaged(cipher_text);
    damaged[i] ^= 0x01;
    try {
      aead_decrypt(damaged, key, "leaf:name");
      cout << "AEAD accepted a change at byte " << i << endl;
      errors++;
      break;
    } catch (const runtime_error &) {
    }
  }
  return errors;
}
}

int main() {
  cout << "Testing crypt.cpp" << endl;

  mode(Verbose, false);
  mode(Testing, true);

  int err_count = 0;
  vector_string messages = test_text();
  err_count = count_if(messages.begin(), messages.end(), test_message_digest);
  err_count += count_if(messages.begin(), messages.end(), test_encryption);
  err_count += test_lengths();
  err_count += test_kdf();
  err_count += test_subkeys();
  err_count += count_if(messages.begin(), messages.end(), test_aead);

  if (err_count)
    cout << "Errors (" << err_count << ") in test!!" << endl;
  else
    cout << "All tests passed!" << endl;
  return 0 != err_count;
}
