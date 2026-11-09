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

#include <iostream>
#include <unistd.h>
#include <sstream>
#include <sys/types.h>

#include "srd.h"

using namespace srd;
using namespace std;

namespace {}

int main() {
  cout << "Testing file_util.cpp" << endl;

  mode(Verbose, false);
  mode(Testing, true);

  int err_count = 0;

  ostringstream tmp_name_s;
  tmp_name_s << "/tmp/srd-" << getpid() << "-" << time(0) << "XXXXXX";
  string tmp_name = tmp_name_s.str();

  for (const string name : {"abc+DEF_123=", "0123456789abcdef"})
    if (!is_safe_file_name(name)) {
      cout << "is_safe_file_name rejected \"" << name << "\"" << endl;
      err_count++;
    }
  for (const string name : {"", "a/b", "/etc/passwd", ".", "..", "../x",
                            "srd.header", "abc.new", "a\nb", "a\x1b[2Jb"})
    if (is_safe_file_name(name)) {
      cout << "is_safe_file_name accepted \"" << name << "\"" << endl;
      err_count++;
    }

  return 0 != err_count;
}
