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



#ifndef __INTERFACE_H__
#define __INTERFACE_H__ 1


#include <map>
#include <string>
#include <vector>

#include "leaf_proxy.h"
#include "types.h"



namespace srd {

        srd::vector_string
                filter_to_keys(const std::string password,
                               const srd::vector_string match_key,
                               const srd::vector_string match_data,
                               const srd::vector_string match_all,
                               const bool match_exact);
        

        std::map<std::string, std::string>
                filter_to_records(const std::string password,
                                  const srd::vector_string match_key,
                                  const srd::vector_string match_data,
                                  const srd::vector_string match_all,
                                  const bool match_exact);

}


#endif  /* __INTERFACE_H__*/
