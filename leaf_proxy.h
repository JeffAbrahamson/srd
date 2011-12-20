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



#ifndef __LEAF_PROXY_H__
#define __LEAF_PROXY_H__ 1


#include <string>

#include "leaf.h"
#include "types.h"



namespace srd {

        /*
          A leaf, meaning a data node, but not necessarily loaded.
          The actual data handling and persistence is done by the leaf
          class.  We make a pointer to a leaf when we load one, and we
          delete the leaf object and null the pointer when we don't
          have it loaded.
        */
        class leaf_proxy {

        public:
                leaf_proxy();   // needed by std::map::operator[]()
                leaf_proxy(const std::string password,
                           const std::string base_name = std::string(),
                           const std::string dir_name = std::string());
                /*
                  Deleting the leaf pointer will cause the leaf to
                  persist if appropriate.  It would be perverse to
                  have multiple leaf_proxy's for the same leaf and
                  also to modify more than one of them.  So we don't
                  go to the effort to make sure this doesn't happen.
                  It's perfectly reasonable to have multiple
                  leaf_proxy's for the same leaf as long as no more
                  than one is considered writeable.
                */
                ~leaf_proxy() { if(the_leaf) delete the_leaf; the_leaf = NULL; };

                leaf_proxy(const leaf_proxy &);
                leaf_proxy &operator=(const leaf_proxy &);

                /*
                  All this const messiness is sad, but hard to fix today.
                  It's because calling key() can load the leaf itself, which
                  means this isn't const.

                  We probably only need operator<() for the compare in
                  leaf_proxy_map.  Provide them all to avoid some
                  subtle bug some day.
                */
                bool operator<=(const leaf_proxy& rhs_in) const
                {
                        leaf_proxy *lhs_ptr = const_cast<leaf_proxy *>(this);
                        leaf_proxy &rhs = const_cast<leaf_proxy &>(rhs_in);
                        return lhs_ptr->key() <= rhs.key();
                }

                bool operator<(const leaf_proxy& rhs_in) const
                {
                        leaf_proxy *lhs_ptr = const_cast<leaf_proxy *>(this);
                        leaf_proxy &rhs = const_cast<leaf_proxy &>(rhs_in);
                        return lhs_ptr->key() < rhs.key();
                }
        
                bool operator>=(const leaf_proxy& rhs_in) const
                { 
                        leaf_proxy *lhs_ptr = const_cast<leaf_proxy *>(this);
                        leaf_proxy &rhs = const_cast<leaf_proxy &>(rhs_in);
                        return lhs_ptr->key() >= rhs.key();
                }

                bool operator>(const leaf_proxy& rhs_in) const
                {
                        leaf_proxy *lhs_ptr = const_cast<leaf_proxy *>(this);
                        leaf_proxy &rhs = const_cast<leaf_proxy &>(rhs_in);
                        return lhs_ptr->key() > rhs.key();
                }


                void key_cache(const std::string in) { cached_key = in; validate(); }
                void key(const std::string);
                const std::string key();
                void payload(const std::string);
                const std::string payload();

                void print_key();
                void print_payload(const std::string payload);

                const std::string basename();
                void commit();
                void erase();

                void validate() const;

        private:

                void init_leaf();
                
                std::string password; // should be const but for operator=()
                std::string base_name;
                std::string dir_name;
                bool valid;

                // We cache the_leaf.key in cached_key so that we don't need to
                // load all leaves for what is likely the most common type of search.
                // This means that the root must persist the cached key value.
                std::string cached_key;
                leaf *the_leaf;
        };


        /*
          A helper object for finding leaves (via leaf_proxy's) that
          match given criteria.

          We are relatively simple, supporting only conjunction
          against key and/or conjunction against payload.
         */
        class leaf_matcher {

        public:
                leaf_matcher(const srd::vector_string,
                             const srd::vector_string,
                             bool conj);
                
                bool operator()(leaf_proxy &);

        private:
                srd::vector_string key;
                srd::vector_string payload;
                bool conjunction;
        };

}

#endif  /* __LEAF_PROXY_H__*/
