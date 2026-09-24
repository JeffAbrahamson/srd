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

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "srd.h"

using namespace srd;
using namespace std;

/*
  Create an empty file.
  It is an error if the file already exists.
*/
void srd::file_create(const string &fn) {
  int fd = creat(fn.c_str(), 0600);
  if (-1 == fd)
    throw(runtime_error(string("Failed to create ") + fn + " :  " +
                        strerror(errno)));
  close(fd);
}

/*
  Write all of data to fd, retrying on short writes and EINTR.
  The filename is only for error messages.
*/
void srd::file_write_all(int fd, const string &data, const string &fn) {
  const char *p = data.data();
  size_t remaining = data.size();
  while (remaining > 0) {
    ssize_t n = write(fd, p, remaining);
    if (n < 0) {
      if (EINTR == errno)
        continue;
      throw(runtime_error(string("Failed to write ") + fn + " :  " +
                          strerror(errno)));
    }
    p += n;
    remaining -= n;
  }
}

/*
  Create a new file that only its owner may read or write, and fill
  it with data.  It is an error if the file already exists or is a
  symbolic link, so we can't be tricked into writing elsewhere.
*/
void srd::file_create_private(const string &fn, const string &data) {
  int fd = open(fn.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (-1 == fd)
    throw(runtime_error(string("Failed to create ") + fn + " :  " +
                        strerror(errno)));
  try {
    file_write_all(fd, data, fn);
  } catch (...) {
    close(fd);
    throw;
  }
  if (close(fd))
    throw(runtime_error(string("Failed to close ") + fn + " :  " +
                        strerror(errno)));
}

/*
  Return true if file exists, false otherwise.
  Throw a hopefully descriptive error on error.
*/
bool srd::file_exists(const string &fn) {
  struct stat buf;
  if (stat(fn.c_str(), &buf)) {
    int error = errno;
    if (ENOENT == errno)
      return false;
    throw(runtime_error(strerror(error)));
  }
  return true;
}

/*
  Remove a file.
*/
void srd::file_rm(const string &fn) {
  int rm_ret = unlink(fn.c_str());
  if (rm_ret) {
    cerr << "  Error removing file:  " << strerror(errno) << endl;
    cerr << "    [File=" << fn << "]" << endl;
  }
}
