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



#ifndef __LEAF_PROXY_MAP_H__
#define __LEAF_PROXY_MAP_H__ 1

#include <map>

#include "types.h"


namespace srd {

        /*
          This is the fundamental data structure of a root node.
          It's also how we pass results sets around so that we
          can continue to filter on them.

          The key is the base_name for a leaf_proxy.
          The associated value is a pointer to a leaf proxy.
          The file_name is generated arbitrarily when a leaf_proxy
          is added, so that changing the key or its payload does not affect the
          root node.  Thus, this map's keys is what we persist to the root file
          (cf. root.h, root.cpp).

          This is a separate class so that we can filter on search results.
        */
        class leaf_proxy_map : public std::map<std::string, leaf_proxy> {
        public:
                leaf_proxy_map filter_keys(srd::vector_string);
                leaf_proxy_map filter_payloads(srd::vector_string) ;
                leaf_proxy_map filter_keys_and_payloads(srd::vector_string,
                                                        srd::vector_string);
                leaf_proxy_map filter_keys_or_payloads(srd::vector_string,
                                                       srd::vector_string);
        };
}

#endif  /* __LEAF_PROXY_MAP_H__*/
