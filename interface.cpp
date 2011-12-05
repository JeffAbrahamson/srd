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



/*
  Here find a public interface for libsrd.so.
*/


#include <map>
#include <string>
#include <vector>

#include "interface.h"
#include "root.h"
#include "types.h"


/*
  In test mode, we'll use a local data directory whose name is based
  on the password.
*/
static bool testing = false;


void srd::test_mode(bool in)
{
        testing = in;
}



bool srd::test_mode()
{
        return testing;
}



srd::vector_string srd::filter_keys(const std::string password,
                                    const srd::vector_string match_key,
                                    const srd::vector_string match_data,
                                    const srd::vector_string match_all,
                                    const bool match_exact,
                                    const bool verbose)
{
        //srd::node_root root;
        
        srd::vector_string v; // ################
        return v;
}


std::map<std::string, std::string> srd::filter_records(const std::string password,
                                                       const srd::vector_string match_key,
                                                       const srd::vector_string match_data,
                                                       const srd::vector_string match_all,
                                                       const bool match_exact,
                                                       const bool verbose)
{
        std::map<std::string, std::string> m; // ################
        return m;
}


