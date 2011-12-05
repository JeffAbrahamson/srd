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



#ifndef  __LEAF_H__
#define __LEAF_H__ 1


#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <string>

#include "compress.h"
#include "crypt.h"
#include "file.h"

#include <assert.h>

namespace srd {

        /*
          Represent a data node, one of the objects the user thinks of
          as what we do.  On destruction, or when explicitly requested
          to do so, persist our data (via the inherited file object).

          We have a key and a payload.  For purposes of being useful,
          we keep track of the password.  That's admittedly not idea,
          since a core dump would potentially contain the password
          multiple times, which would make it easier to find.  On the
          other hand, it's not the human-readable pass phrase at least.
        */
        class leaf : public file, public compress, public crypt {
        public:
                leaf() { assert(0); };  // seemingly needed by serialize()
                leaf(const std::string password,
                     const std::string base_name = std::string(),
                     const std::string dir_name = std::string(),
                     const bool testing = false);
                virtual ~leaf();

                void commit();
                void erase();
                
                void key(const std::string &key_in) {
                        node_key = key_in;
                        modified = true;
                };
                const std::string key() { return node_key; };
                void payload(const std::string &payload_in)
                {
                        node_payload = payload_in;
                        modified = true;
                };
                const std::string payload() { return node_payload; };

                void validate();
                                
        private:
                friend class boost::serialization::access;
                template<class Archive>
                        void serialize(Archive &ar, const unsigned int version);

                const std::string password;
                bool modified;
                
                std::string node_key;
                std::string node_payload;
        };
        
}


#endif  /* __LEAF_H__*/
