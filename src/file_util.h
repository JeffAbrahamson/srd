/*
  Copyright 2012  Jeff Abrahamson
  
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



#ifndef __FILE_UTIL_H__
#define __FILE_UTIL_H__ 1


#include <string>


namespace srd {
        
        void file_create(const std::string &filename);
        bool file_exists(const std::string &filename);
        void file_rm(const std::string &filename);
        
}


#endif  /* __FILE_UTIL_H__*/
