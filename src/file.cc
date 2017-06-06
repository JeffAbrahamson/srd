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

#include <assert.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "srd.h"

using namespace srd;
using namespace std;

namespace {

string base_dir;

string &get_base_dir() {
  // Make sure it's hard to set after first use
  if (base_dir.empty())
    base_dir = string(getenv("HOME")) + "/srd/";
  return base_dir;
}
}

/*
  Set base directory.  This must be called before anything tries to
  use the base directory.  It may only be called once.
*/
void srd::set_base_dir(const std::string &in_dir) {
  // Illegal to set more than once or to set after use
  assert("" == base_dir);
  if (mode(Verbose))
    cout << "Setting base dir to " << in_dir << endl;
  base_dir = in_dir;
}

/*
  Create a file if we know something about what to call it.
*/
File::File(const string base_name, const string dir_name)
    : m_dir_name(dir_name), m_base_name(base_name), m_dir_verified(false) {}

/*
  Return the name of the directory in which the file lives or will live.
  Compute the name if needed.  Create the directory if needed.
*/
string File::dirname() {
  if ("" == m_dir_name) {
    // Test mode only affects what directory we generate, if asked.
    if (mode(Testing)) {
      ostringstream spath;
      spath << "srd-test-0000-" << getenv("LOGNAME");
      m_dir_name = spath.str();
    } else
      m_dir_name = get_base_dir();
  }
  if (!m_dir_verified && mkdir(m_dir_name.c_str(), 0700) && EEXIST != errno) {
    cerr << "  Error creating directory \"" << m_dir_name
         << "\": " << strerror(errno) << endl;
    throw(runtime_error("Failed to create directory."));
  }
  m_dir_verified = true;
  return m_dir_name;
}

/*
  Return the name by which we will know the file.
  Compute the name if needed.
*/
string File::basename() {
  // if("" == m_base_name) {
  if (m_base_name == "") {
    ostringstream sname;
    sname << getpid() << getppid() << time(NULL);
    sname << pseudo_random_string(20);
    m_base_name = message_digest(sname.str(), true);
  }
  return m_base_name;
}

/*
  Set the contents of the file.
  The file need not yet exist.

  Only this function, when calling itself recursively, should set
  lock = false.  Otherwise, just set data.

*/
void File::file_contents(string &data, bool lock) {
  string lock_filename = full_path() + ".lck";
  if (lock) {
    Lock L(lock_filename);
    file_contents_sub(data);
  } else
    file_contents_sub(data);
}

/*
  Do the guts of setting the contents of a file.
  Abstracted to a separate function to permit RAII
  lock.
*/
void File::file_contents_sub(string &data) {
  string filename = full_path();
  string filename_new = filename + ".new";

  ofstream fs(filename_new.c_str(), ios::out | ios::binary);
  if (!fs.is_open()) {
    char *err_str = strerror(errno);
    ostringstream oss(string("Failed to open file \""));
    oss << filename_new << "\" for writing: " << err_str;
    throw(runtime_error(oss.str()));
  }
  fs.write(data.data(), data.size());
  fs.close();

  rename(filename_new.c_str(), filename.c_str()); // guaranteed atomic
  m_modtime = modtime(false);
}

/*
  Return the contents of the file.
  It is an error for the file not to exist.
*/
string File::file_contents() {
  // There is a race condition here, someone could modify the
  // file between our stat and our read.  The result would be a
  // reread later (if we ever need it).
  m_modtime = modtime(false);

  ifstream fs(full_path().c_str(), ios::in | ios::binary | ios::ate);
  if (!fs.is_open()) {
    ostringstream oss(string("Failed to open file \""));
    oss << full_path() << "\" for reading.";
    throw(runtime_error(oss.str()));
  }
  size_t size = fs.tellg();
  if (0 == size)
    return string();
  char *data = new char[size];
  fs.seekg(0, ios::beg);
  fs.read(data, size);
  fs.close();
  string data_str(data, size);
  return data_str;
}

/*
  Get the modification time of the file.
  If silent is false, complain if the file looks odd,
  notably if it is not a regular file.
*/
time_pair File::modtime(const bool silent) {
  struct stat stat_buf;
  int ret = stat(full_path().c_str(), &stat_buf);
  if (ret) {
    cerr << "Failed to stat " << full_path() << ":" << endl;
    cerr << "  " << strerror(errno) << endl;
    throw(runtime_error("File::modtime() failed to stat file"));
  }
  if (S_ISLNK(stat_buf.st_mode))
    // At issue is that the view from different hosts
    // could be different, depending on whether the link
    // always exists and whether it points to the same
    // place all the time.  If the underlying directory is
    // a symlink, all should be well.
    cout << full_path() << " is a symbolic link, odd things could happen."
         << endl;
  else if (!S_ISREG(stat_buf.st_mode))
    cout << full_path() << " is not a regular file, odd things could happen."
         << endl;

#if defined __USE_MISC || defined __USE_XOPEN2K8
  return time_pair(stat_buf.st_mtim.tv_sec, stat_buf.st_mtim.tv_nsec);
#else
  return time_pair(stat_buf.st_mtime, stat_buf.st_mtimensec);
#endif
}

/*
  Return true if file is modified since last we read it, false
  otherwise.
*/
bool File::underlying_is_modified() {
  time_pair mt(modtime());
  if (mt > m_modtime)
    return true;
  return false;
}

/*
  Remove the underlying file.
  It is not an error later to rewrite the file, and the object remains
  valid after calling rm().  Since we don't maintain a file descriptor
  except when we need the file open, calling rm() and then rewriting
  the file will leave a file lying about.
*/
void File::rm() {
  // We're const with respect to the class.
  file_rm(full_path());
}

/*
  Return true if the file already exists, false otherwise.
*/
bool File::exists() { return file_exists(full_path()); }

bool File::is_writeable() {
  int ret = access(full_path().c_str(), W_OK);
  if (ret) {
    if (ENOENT == errno)
      return dir_is_writeable();
    if (EROFS == errno || EACCES == errno)
      return false;
    string errstr(strerror(errno));
    cerr << "[file] access(" << full_path() << "):" << errno << endl;
    cerr << "  " << errstr << endl;
    return false;
  }
  return true;
}

bool File::dir_is_writeable() {
  int ret = access(dirname().c_str(), W_OK);
  if (ret) {
    if (EROFS == errno || EACCES == errno)
      return false;
    string errstr(strerror(errno));
    cerr << "[dir] access(" << full_path() << "):" << errno << endl;
    cerr << "  " << errstr << endl;
    return false;
  }
  return true;
}
