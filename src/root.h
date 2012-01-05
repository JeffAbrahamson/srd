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

          If create is false, the root's underlying file must already exist.
          If it is true, the root file must not exist and is created.
        */
        class Root : public File, public Compress, public Crypt, public LeafProxyMap {
        public:
                Root(const std::string password,
                     const std::string path = std::string(),
                     const bool create = false);
                virtual ~Root();

                // A leaf (or leaf proxy) contains a key and payload.
                // But the root node contains proxy keys, which serve to identify
                // the actual leaf, in which we have stored the real key.
                void add_leaf(const std::string key, const std::string payload);
                LeafProxy get_leaf(const std::string proxy_key);
                void set_leaf(const std::string proxy_key,
                              const std::string key,
                              const std::string payload);
                void rm_leaf(const std::string proxy_key);

                Root change_password(const std::string new_password);
                void commit();
                void validate();

        private:
                
                friend class boost::serialization::access;
                template<class Archive>
                        void serialize(Archive &ar, const unsigned int version);

                // Used for serialization only.  A kludge.
                class LeafProxyPersist {
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
                std::vector<LeafProxyPersist> leaf_names;

                void instantiate_leaf_proxy(LeafProxyPersist proxy_info);
                void populate_leaf_names(LeafProxyMap::value_type val);
                
                // Data members

                const std::string password;
                bool modified;
                bool valid;     // if false, all operations except deletion should fail
        };
}

#endif  /* __ROOT_H__*/
