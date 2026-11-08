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
#include <crypto++/base64.h>
#include <crypto++/osrng.h>
#include <crypto++/sha.h>
#include <errno.h>
#include <sodium.h>
#include <sstream>
#include <stdexcept>

// Modified from http://www.cryptopp.com/wiki/Hash_Functions
//
// Block Cipher Headers
#include <crypto++/aes.h>
// #include <crypto++/blowfish.h>
// #include <crypto++/camellia.h>
// #include <crypto++/cast.h>
// #include <crypto++/des.h>
// #include <crypto++/gost.h>
// #include <crypto++/idea.h>
// #include <crypto++/mars.h>
// #include <crypto++/rc2.h>
// #include <crypto++/rc5.h>
// #include <crypto++/rc6.h>
// #include <crypto++/rijndael.h>
// #include <crypto++/safer.h>
// #include <crypto++/serpent.h>
// #include <crypto++/shacal2.h>
// #include <crypto++/shark.h>
// #include <crypto++/skipjack.h>
// #include <crypto++/tea.h>
// #include <crypto++/3way.h>
// #include <crypto++/twofish.h>
// #include <crypto++/tea.h>

// C Runtime Includes
#include <iostream>
#include <iomanip>

// Crypto++ Includes
// #include <cryptlib.h>
#include <crypto++/modes.h>   // xxx_Mode< >
#include <crypto++/filters.h> // StringSource and
// StreamTransformation

// Ciphers and cipher modes are types, so have to use cpp, pending inspiration.
// Ick.  Sorry.
//
// Cipher Modes
//
// #define CIPHER_MODE CBC_CTS_Mode
#define CIPHER_MODE CBC_Mode
// #define CIPHER_MODE CFB_FIPS_Mode
// #define CIPHER_MODE CFB_Mode
// #define CIPHER_MODE CTR_Mode
// #define CIPHER_MODE ECB_Mode
// #define CIPHER_MODE OFB_Mode

// Ciphers
//
#define CIPHER AES
// #define CIPHER Blowfish
// #define CIPHER BTEA
// #define CIPHER Camellia
// #define CIPHER CAST128
// #define CIPHER CAST256
// #define CIPHER DES
// #define CIPHER DES_EDE2
// #define CIPHER DES_EDE3
// #define CIPHER DES_XEX3
// #define CIPHER GOST
// #define CIPHER IDEA
// #define CIPHER MARS
// #define CIPHER RC2
// #define CIPHER RC5
// #define CIPHER RC6
// #define CIPHER Rijndael
// #define CIPHER SAFER_K
// #define CIPHER SAFER_SK
// #define CIPHER Serpent
// #define CIPHER SHACAL2
// #define CIPHER SHARK
// #define CIPHER SKIPJACK
// #define CIPHER ThreeWay
// #define CIPHER Twofish
// #define CIPHER XTEA

#include "srd.h"

using namespace srd;
using namespace std;

/*
  Compute hash (SHA-256) of a message.  Return base64-encoded string
  of hash.  We use this notably to transform a passphrase to a
  password.

  We also use this to generate pseudorandom filenames, in which case
  the filesystem_safe flag avoids embedded '/'.
*/
string srd::message_digest(const string &message, bool filesystem_safe) {
  string digest;
  CryptoPP::SHA256 hash;
  CryptoPP::StringSource(
      message, true,
      new CryptoPP::HashFilter(
          hash, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(digest))));

  // Why does a LF get appended?  Just hack around it.
  if (*digest.rbegin() == '\n')
    digest.erase(digest.end() - 1);
  if (!filesystem_safe)
    return digest;

  string safe_name;
  replace_copy(digest.begin(), digest.end(), back_inserter(safe_name), '/',
               '_');
  return safe_name;
}

/*
  Return a string of length pseudo-random characters.
  Not necessarily human readable.
*/
string srd::pseudo_random_string(int length) {
  CryptoPP::AutoSeededRandomPool rng;
  string rand_str(length, '\0');
  rng.GenerateBlock(reinterpret_cast<CryptoPP::byte *>(rand_str.data()),
                    length);
  return rand_str;
}

typedef CryptoPP::byte crypto_key_type[CryptoPP::CIPHER::DEFAULT_KEYLENGTH];
typedef CryptoPP::byte crypto_iv_type[CryptoPP::CIPHER::BLOCKSIZE];

namespace {

/*
  Initialize the key and iv given the password.
*/
void init_key_iv(const string password, crypto_key_type &key,
                 crypto_iv_type &iv) {
  // Key and IV setup.
  // IV is just hash of key
  bzero(key, CryptoPP::CIPHER::DEFAULT_KEYLENGTH);
  size_t key_len = min(static_cast<size_t>(CryptoPP::CIPHER::DEFAULT_KEYLENGTH),
                       password.size());
  memcpy(key, password.c_str(), key_len);

  bzero(iv, CryptoPP::CIPHER::BLOCKSIZE);
  string iv_string = message_digest(password);
  size_t iv_len = min(static_cast<size_t>(CryptoPP::CIPHER::BLOCKSIZE),
                      iv_string.size());
  memcpy(iv, iv_string.c_str(), iv_len);
}
}

/*
  Encrypt and return cipher text.
*/
string srd::format1_encrypt(const string &plain_text, const string &password) {
  try {
    crypto_key_type key;
    crypto_iv_type iv;
    init_key_iv(password, key, iv);

    // Cipher Text Sink
    string cipher_text;

    // Encryptor
    CryptoPP::CIPHER_MODE<CryptoPP::CIPHER>::Encryption Encryptor(
        key, sizeof(key), iv);

    // Encryption
    CryptoPP::StreamTransformationFilter *source =
        new CryptoPP::StreamTransformationFilter(
            Encryptor,
            new CryptoPP::StringSink(
                cipher_text)); // StreamTransformationFilter
    CryptoPP::StringSource(plain_text, true, source);
    return cipher_text;

  } catch (CryptoPP::Exception &e) {
    cerr << e.what() << endl;
  }

  catch (...) {
    cerr << "Unknown Error" << endl;
  }
  throw(runtime_error("encryption failed"));
}

/*
  Decrypt and return plain text.
*/
string srd::format1_decrypt(const string &cipher_text, const string &password) {
  try {
    crypto_key_type key;
    crypto_iv_type iv;
    init_key_iv(password, key, iv);

    // Recovered Text Sink
    string plain_text;

    // Decryptor
    CryptoPP::CIPHER_MODE<CryptoPP::CIPHER>::Decryption Decryptor(
        key, sizeof(key), iv);

    // Decryption
    CryptoPP::StringSource(
        cipher_text, true,
        new CryptoPP::StreamTransformationFilter(
            Decryptor,
            new CryptoPP::StringSink(plain_text)) // StreamTransformationFilter
        );                                        // StringSource
    return plain_text;
  } catch (CryptoPP::Exception &e) {
    cerr << e.what() << endl;
  }

  catch (...) {
    cerr << "Unknown Error" << endl;
  }
  throw(runtime_error("decryption failed"));
}

/* ************************************************************ */
/* Format 2 primitives (libsodium) */

namespace {

// The first bytes of every format 2 encrypted file.
const string aead_magic("SRD2");

// Context for crypto_kdf.  Exactly crypto_kdf_CONTEXTBYTES long.
const char kdf_context[] = "srd-keys";
static_assert(sizeof(kdf_context) - 1 == crypto_kdf_CONTEXTBYTES);

// Bounds on Argon2id costs read from a database.  The floor stops a
// tampered header from making password guesses cheap; the ceiling
// stops it from making srd exhaust memory.
const uint64_t opslimit_floor = crypto_pwhash_argon2id_OPSLIMIT_INTERACTIVE;
const uint64_t memlimit_floor = crypto_pwhash_argon2id_MEMLIMIT_INTERACTIVE;
const uint64_t opslimit_ceiling = 16;
const uint64_t memlimit_ceiling = 4ULL << 30; // 4 GiB

void init_sodium() {
  static const bool ok = sodium_init() >= 0;
  if (!ok)
    throw(runtime_error("Failed to initialize libsodium."));
}

const unsigned char *bytes(const string &s) {
  return reinterpret_cast<const unsigned char *>(s.data());
}

unsigned char *bytes(string &s) {
  return reinterpret_cast<unsigned char *>(s.data());
}
}

KdfParams srd::new_kdf_params() {
  init_sodium();
  KdfParams params;
  if (mode(Testing)) {
    // The test suite creates many databases; keep that fast.
    params.opslimit = crypto_pwhash_argon2id_OPSLIMIT_MIN;
    params.memlimit = crypto_pwhash_argon2id_MEMLIMIT_MIN;
  } else {
    params.opslimit = crypto_pwhash_argon2id_OPSLIMIT_MODERATE;
    params.memlimit = crypto_pwhash_argon2id_MEMLIMIT_MODERATE;
  }
  params.salt.resize(crypto_pwhash_argon2id_SALTBYTES);
  randombytes_buf(bytes(params.salt), params.salt.size());
  return params;
}

void srd::check_kdf_params(const KdfParams &params) {
  if (params.salt.size() != crypto_pwhash_argon2id_SALTBYTES)
    throw(runtime_error("Database header has a salt of the wrong size."));
  // Test databases use the minimum costs, so only test mode may
  // open them.
  const uint64_t ops_min =
      mode(Testing) ? crypto_pwhash_argon2id_OPSLIMIT_MIN : opslimit_floor;
  const uint64_t mem_min =
      mode(Testing) ? crypto_pwhash_argon2id_MEMLIMIT_MIN : memlimit_floor;
  if (params.opslimit < ops_min || params.memlimit < mem_min)
    throw(runtime_error("Database header asks for a password hash that is "
                        "too weak.  (Has it been tampered with?)"));
  if (params.opslimit > opslimit_ceiling || params.memlimit > memlimit_ceiling)
    throw(runtime_error("Database header asks for a password hash that is "
                        "too costly.  (Has it been tampered with?)"));
}

string srd::derive_master_key(const string &passphrase,
                              const KdfParams &params) {
  init_sodium();
  check_kdf_params(params);
  string key(crypto_kdf_KEYBYTES, '\0');
  if (crypto_pwhash(bytes(key), key.size(), passphrase.data(),
                    passphrase.size(), bytes(params.salt), params.opslimit,
                    params.memlimit, crypto_pwhash_ALG_ARGON2ID13) != 0)
    throw(runtime_error("Password hashing failed (out of memory?)."));
  return key;
}

string srd::derive_subkey(const string &master_key, SubkeyId id) {
  init_sodium();
  if (master_key.size() != crypto_kdf_KEYBYTES)
    throw(runtime_error("Master key has the wrong size."));
  string subkey(crypto_aead_xchacha20poly1305_ietf_KEYBYTES, '\0');
  static_assert(crypto_aead_xchacha20poly1305_ietf_KEYBYTES >=
                    crypto_kdf_BYTES_MIN &&
                crypto_aead_xchacha20poly1305_ietf_KEYBYTES <=
                    crypto_kdf_BYTES_MAX);
  crypto_kdf_derive_from_key(bytes(subkey), subkey.size(), id, kdf_context,
                             bytes(master_key));
  return subkey;
}

string srd::keyed_name(const string &key, const string &label) {
  init_sodium();
  unsigned char hash[32];
  crypto_generichash(hash, sizeof(hash), bytes(label), label.size(), bytes(key),
                     key.size());
  char hex[2 * sizeof(hash) + 1];
  sodium_bin2hex(hex, sizeof(hex), hash, sizeof(hash));
  return hex;
}

/*
  Format:  magic | nonce | ciphertext and tag.  The magic and the
  context are the associated data.
*/
string srd::aead_encrypt(const string &plain_text, const string &key,
                         const string &context) {
  init_sodium();
  if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES)
    throw(runtime_error("Encryption key has the wrong size."));
  const size_t nonce_len = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  string cipher_text(aead_magic.size() + nonce_len + plain_text.size() +
                         crypto_aead_xchacha20poly1305_ietf_ABYTES,
                     '\0');
  copy(aead_magic.begin(), aead_magic.end(), cipher_text.begin());
  unsigned char *nonce = bytes(cipher_text) + aead_magic.size();
  randombytes_buf(nonce, nonce_len);
  const string ad = aead_magic + context;
  unsigned long long body_len;
  crypto_aead_xchacha20poly1305_ietf_encrypt(
      nonce + nonce_len, &body_len, bytes(plain_text), plain_text.size(),
      bytes(ad), ad.size(), nullptr, nonce, bytes(key));
  assert(aead_magic.size() + nonce_len + body_len == cipher_text.size());
  return cipher_text;
}

string srd::aead_decrypt(const string &cipher_text, const string &key,
                         const string &context) {
  init_sodium();
  if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES)
    throw(runtime_error("Encryption key has the wrong size."));
  const size_t nonce_len = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  const size_t overhead =
      aead_magic.size() + nonce_len + crypto_aead_xchacha20poly1305_ietf_ABYTES;
  if (cipher_text.size() < overhead ||
      cipher_text.compare(0, aead_magic.size(), aead_magic) != 0)
    throw(runtime_error("Not an srd file, or truncated."));
  const unsigned char *nonce = bytes(cipher_text) + aead_magic.size();
  const size_t body_len = cipher_text.size() - aead_magic.size() - nonce_len;
  string plain_text(cipher_text.size() - overhead, '\0');
  const string ad = aead_magic + context;
  unsigned long long plain_len;
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          bytes(plain_text), &plain_len, nullptr, nonce + nonce_len, body_len,
          bytes(ad), ad.size(), nonce, bytes(key)) != 0)
    throw(runtime_error("Decryption failed:  the file is damaged or has "
                        "been changed."));
  assert(plain_len == plain_text.size());
  return plain_text;
}

/* ************************************************************ */
/* Format 1 password and keys */

/*
  Format 1 hashed the passphrase with 50 rounds of SHA-256.  In test
  mode, the passphrase on the command line was used as is, and old
  test databases (test.d/regression-db.tgz) depend on that.
*/
string srd::format1_password(const string &passphrase) {
  if (mode(Testing))
    return passphrase;
  // Wipe each intermediate digest when done with it, to minimize the
  // risk of seeing them in a swap or core image.
  string digest = message_digest(passphrase, false);
  for (int i = 1; i < 50; i++) {
    string next = message_digest(digest, false);
    sodium_memzero(digest.data(), digest.size());
    digest.swap(next);
  }
  return digest;
}

Key::~Key() { sodium_memzero(m_secret.data(), m_secret.size()); }

string Key::encrypt(const string &plain_text, const string &context) const {
  if (Format1 == m_format)
    return format1_encrypt(plain_text, m_secret);
  return aead_encrypt(plain_text, m_secret, context);
}

string Key::decrypt(const string &cipher_text, const string &context) const {
  if (Format1 == m_format)
    return format1_decrypt(cipher_text, m_secret);
  return aead_decrypt(cipher_text, m_secret, context);
}
