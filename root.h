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



#ifndef  __ROOT_H__
#define __ROOT_H__ 1


#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <string>
#include <vector>

#include "compress.h"
#include "crypt.h"
#include "file.h"
#include "leaf_proxy.h"
#include "leaf_proxy_map.h"
//#include "types.h"


namespace srd {

        /*
          The root node, which persists to a file, tracks the leaf nodes.
          The thing we actually persist is just the set of leaf names.
          At runtime, the thing we have is a map from leaf names to leaf proxy objects.

          The root node supports filter operations on the leaf proxies (and
          so on the leaf objects themselves).
        */
        class root : public file, public compress, public crypt, public leaf_proxy_map {
        public:
                root(const std::string password,
                     const std::string path = std::string());
                virtual ~root();

                // A leaf (or leaf proxy) contains a key and payload.
                // But the root node contains proxy keys, which serve to identify
                // the actual leaf, in which we have stored the real key.
                void add_leaf(const std::string key, const std::string payload);
                leaf_proxy get_leaf(const std::string proxy_key);
                void set_leaf(const std::string proxy_key,
                              const std::string key,
                              const std::string payload);
                void rm_leaf(const std::string proxy_key);

                // void change_password(const std::string new_password);
                void commit();
                void validate();

        private:
                
                friend class boost::serialization::access;
                template<class Archive>
                        void serialize(Archive &ar, const unsigned int version);

                // Used for serialization only.  A kludge.
                class leaf_proxy_persist {
                public:
                        std::string proxy_name;
                        std::string cached_key;
                private:
                        friend class boost::serialization::access;
                        template<class Archive>
                                void serialize(Archive &ar, const unsigned int version)
                                {
                                        ar & proxy_name & cached_key;
                                }
                };
                std::vector<leaf_proxy_persist> leaf_names;

                void instantiate_leaf_proxy(leaf_proxy_persist proxy_info);
                void populate_leaf_names(leaf_proxy_map::value_type val);
                
                // Data members

                const std::string password;
                bool modified;
        };
}

#endif  /* __ROOT_H__*/
