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

#include <sodium.h>
#include <stdexcept>
#include <string>

#include "srd.h"

using namespace srd;
using namespace std;

/*
  The name contains a '.', which no root or leaf name does, so it
  can't collide with them.
*/
const string Header::file_name("srd.header");

namespace {

/*
  Layout, integers little-endian:

    "SRDH"      magic, 4 bytes
    version     1 byte, 2 (the database format)
    algorithm   1 byte, crypto_pwhash_ALG_ARGON2ID13
    opslimit    4 bytes
    memlimit    8 bytes
    salt        crypto_pwhash_argon2id_SALTBYTES bytes
*/
const string magic("SRDH");
const unsigned char version = 2;
const size_t header_size = magic.size() + 1 + 1 + 4 + 8 +
                           crypto_pwhash_argon2id_SALTBYTES;

void put_le(string &out, uint64_t value, int n_bytes) {
  for (int i = 0; i < n_bytes; i++)
    out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
}

uint64_t get_le(const string &in, size_t pos, int n_bytes) {
  uint64_t value = 0;
  for (int i = 0; i < n_bytes; i++)
    value |= static_cast<uint64_t>(static_cast<unsigned char>(in[pos + i]))
             << (8 * i);
  return value;
}
}

string Header::encode(const KdfParams &params) {
  if (params.salt.size() != crypto_pwhash_argon2id_SALTBYTES ||
      params.opslimit > 0xffffffffULL)
    throw(logic_error("Can't encode these key derivation parameters."));
  string out(magic);
  out.push_back(static_cast<char>(version));
  out.push_back(static_cast<char>(crypto_pwhash_ALG_ARGON2ID13));
  put_le(out, params.opslimit, 4);
  put_le(out, params.memlimit, 8);
  out += params.salt;
  return out;
}

KdfParams Header::decode(const string &data) {
  if (data.size() != header_size || data.compare(0, magic.size(), magic))
    throw(runtime_error("The database header (" + file_name +
                        ") is damaged."));
  size_t pos = magic.size();
  if (static_cast<unsigned char>(data[pos]) != version)
    throw(runtime_error("The database header (" + file_name +
                        ") is for an unknown format.  (Is this srd too "
                        "old?)"));
  pos++;
  if (static_cast<unsigned char>(data[pos]) != crypto_pwhash_ALG_ARGON2ID13)
    throw(runtime_error("The database header (" + file_name +
                        ") names an unknown password hash."));
  pos++;
  KdfParams params;
  params.opslimit = get_le(data, pos, 4);
  pos += 4;
  params.memlimit = get_le(data, pos, 8);
  pos += 8;
  params.salt = data.substr(pos);
  return params;
}

KdfParams Header::params() {
  KdfParams params = decode(file_contents());
  check_kdf_params(params);
  return params;
}

KdfParams Header::params_or_create() {
  // Two srd processes creating databases at once must agree on one
  // header.
  Lock lock(dirname());
  if (exists())
    return params();
  KdfParams params = new_kdf_params();
  file_contents(encode(params));
  return params;
}
