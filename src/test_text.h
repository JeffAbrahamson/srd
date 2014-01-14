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



#ifndef __TEST_TEXT_H__
#define __TEST_TEXT_H__ 1

#include <map>
#include <vector>
#include <string>

#include "srd.h"


namespace srd {
        
    srd::vector_string test_text();
    std::map<std::string, std::string> orderly_text();
    std::map<std::string, std::string> case_text();
}


#endif  /* __TEST_TEXT_H__*/
