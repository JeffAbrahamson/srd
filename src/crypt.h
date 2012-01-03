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



#ifndef  __CRYPT_H__
#define __CRYPT_H__ 1


#include<string>


namespace srd {

        // Compute a hash (message digest).
        std::string message_digest(const std::string &message,
                                   const bool filesystem_safe = false);

        // Return a (not necessarily human readable) string of random bits.
        std::string pseudo_random_string(int length);
        
        // Crypto++ is documented at http://www.cryptopp.com/
        // and http://www.cryptopp.com/fom-serve/cache/1.html
        // and http://www.cryptopp.com/wiki/FAQ

        /*
          This is a simple mixin class that provides encryption and decryption.
        */
        class crypt {
        public:
                std::string encrypt(const std::string plain_message,
                                    const std::string password);
                std::string decrypt(const std::string cipher_message,
                                    const std::string password);
        };
}

#endif  /* __CRYPT_H__*/
