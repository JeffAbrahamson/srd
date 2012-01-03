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
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

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
#include <crypto++/modes.h> // xxx_Mode< >
#include <crypto++/filters.h> // StringSource and
// StreamTransformation

// Ciphers and cipher modes are types, so have to use cpp, pending inspiration.  Ick.  Sorry.
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

#include "crypt.h"


using namespace srd;
using namespace std;



/*
  Compute hash (SHA-256) of a message.  Return base64-encoded string
  of hash.  We use this notably to transform a passphrase to a
  password.

  We also use this to generate pseudorandom filenames, in which case
  the filesystem_safe flag avoids embedded '/'.
*/
string srd::message_digest(const string &message, bool filesystem_safe)
{
        string digest;
        CryptoPP::SHA256 hash;
        CryptoPP::StringSource(message, true,
                               new CryptoPP::HashFilter(hash,
                                                        new CryptoPP::Base64Encoder(new CryptoPP::StringSink(digest))));

        // Why does a LF get appended?  Just hack around it.
        if(*digest.rbegin() == '\n')
                digest.erase(digest.end() - 1);
        if(!filesystem_safe)
                return digest;
        
        string safe_name;
        replace_copy(digest.begin(),
                     digest.end(),
                     back_inserter(safe_name),
                     '/',
                     '_');
        return safe_name;
}



/*
  Return a string of length pseudo-random characters.
  Not necessarily human readable.
*/
string srd::pseudo_random_string(int length)
{
        CryptoPP::AutoSeededRandomPool rng;
        byte random_bytes[length];
        rng.GenerateBlock(random_bytes, length);
        string rand_str;
        copy(random_bytes, random_bytes + length, back_inserter(rand_str));
        return rand_str;
}



typedef byte crypto_key_type[CryptoPP::CIPHER::DEFAULT_KEYLENGTH];
typedef byte crypto_iv_type[CryptoPP::CIPHER::BLOCKSIZE];



namespace {

        /*
          Initialize the key and iv given the password.
        */
        void init_key_iv(const string password, crypto_key_type &key, crypto_iv_type &iv)
        {
                // Key and IV setup.
                // IV is just hash of key
                bzero(key, CryptoPP::CIPHER::DEFAULT_KEYLENGTH);
                size_t key_len = min(static_cast<size_t>(CryptoPP::CIPHER::DEFAULT_KEYLENGTH),
                                     password.size());
                memcpy(key, password.c_str(), key_len);
                
                bzero(iv, CryptoPP::CIPHER::BLOCKSIZE);
                string iv_string = message_digest(password);
                size_t iv_len = min(static_cast<size_t>(CryptoPP::CIPHER::DEFAULT_KEYLENGTH),
                                    iv_string.size());
                memcpy(iv, iv_string.c_str(), iv_len);
        }
}


/*
  Encrypt and return cipher text.
*/
string crypt::encrypt(const string plain_text, const string password)
{
        try {
                crypto_key_type key;
                crypto_iv_type iv;
                init_key_iv(password, key, iv);
                
                // Cipher Text Sink
                string cipher_text;

                // Encryptor
                CryptoPP::CIPHER_MODE<CryptoPP::CIPHER>::Encryption
                        Encryptor(key, sizeof(key), iv);

                // Encryption
                CryptoPP::StreamTransformationFilter *source =
                        new CryptoPP::StreamTransformationFilter(Encryptor,
                                                                 new CryptoPP::StringSink(cipher_text)
                                                                 ); // StreamTransformationFilter
                CryptoPP::StringSource(plain_text, true, source);
                return cipher_text;

        }
        catch(CryptoPP::Exception& e) {
                cerr << e.what() << endl;
        }
   
        catch(...) {
                cerr << "Unknown Error" << endl;
        }
        throw(runtime_error("encryption failed"));
}



/*
  Decrypt and return plain text.
*/
string crypt::decrypt(const string cipher_text, const string password)
{
        try {
                crypto_key_type key;
                crypto_iv_type iv;
                init_key_iv(password, key, iv);

                // Recovered Text Sink
                string plain_text;

                // Decryptor
                CryptoPP::CIPHER_MODE<CryptoPP::CIPHER>::Decryption
                        Decryptor(key, sizeof(key), iv);

                // Decryption
                CryptoPP::StringSource(cipher_text, true,
                                       new CryptoPP::StreamTransformationFilter(Decryptor,
                                                                                new CryptoPP::StringSink(plain_text)
                                                                                ) // StreamTransformationFilter
                                       ); // StringSource
                return plain_text;
        }
        catch(CryptoPP::Exception& e) {
                cerr << e.what() << endl;
        }
   
        catch(...) {
                cerr << "Unknown Error" << endl;
        }   
        throw(runtime_error("decryption failed"));
}
