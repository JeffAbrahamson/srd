/*
  Copyright (C) 2004-2008 René Nyffenegger.
  See base64.cpp for full copyright and license.

  Modified 2014 by Jeff Abrahamson.
  Copyright 2014  Jeff Abrahamson
  
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

#include <string>

std::string base64_encode(const std::string& string_to_encode);
std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);
