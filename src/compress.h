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



#ifndef  __COMPRESS_H__
#define __COMPRESS_H__ 1

#include <string>

namespace srd {

        /*
          This is a simple mixin class that provides compression and decompression.
        */
        class Compress {
        public:
                std::string compression(const std::string);
                std::string decompression(const std::string, unsigned int = 0);
                
        };
}

#endif  /* __COMPRESS_H__*/
