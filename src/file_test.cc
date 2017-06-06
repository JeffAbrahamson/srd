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

#include <algorithm>
#include <boost/interprocess/sync/file_lock.hpp>
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <unistd.h>
#include <vector>

#include "srd.h"
#include "test_text.h"

using namespace srd;
using namespace std;

namespace {

int test_exists(const string dir, const string base, bool expect_exists);
int test_file(string message);
int test_modified();

int test_exists(const string dir, const string base, bool expect_exists) {
  File my_file(base, dir);
  try {
    if (expect_exists == my_file.exists())
      return 0;
    return 1;
  } catch (exception e) {
    cerr << dir << "/" << base << ":  " << e.what() << endl;
    return 1;
  }
  cerr << dir << "/" << base << " (existence check failed)" << endl;
  return 1;
}

int test_file(string message) {
  int ret = 0;
  string filename; // Remember so we can clean up no matter what
  try {
    File my_file;
    filename = my_file.full_path();
    my_file.file_contents(message);
    string dup_message(my_file.file_contents());

    if (message != dup_message) {
      cout << "File write + read not identity!" << endl;
      ret++;
    }
  } catch (runtime_error e) {
    cerr << e.what() << endl;
    ret++;
  } catch (...) {
    cerr << "Something unexpected went wrong." << endl;
    ret++;
  }
  int rm_ret = unlink(filename.c_str());
  if (rm_ret) {
    cerr << "  Error removing test file:  " << strerror(errno) << endl;
    cerr << "    (File=" << filename << ")" << endl;
    ret++;
  }

  return ret;
}

/*
  Check that we can tell if a file has been modified.
*/
int test_modified() {
  int err_count = 0;
  File f1;
  File f2(f1.basename(), f1.dirname());

  string contents("This is a test.");
  f1.file_contents(contents);

  string f2_contents = f2.file_contents();
  err_count += (f2.underlying_is_modified() ? 1 : 0); // Nope, hasn't been
  if (err_count)
    cout << "Unmodified file looks modified in test_modified()." << endl;

  sleep(1); // Apparently we can do this in less than a nanosecond if we don't
            // sleep
  contents = "This is a test.";
  f1.file_contents(contents);
  int err2 = (f2.underlying_is_modified() ? 0 : 1); // Now it has been modified
  err_count += err2;
  if (err2)
    cout << "Modified file looks unmodified in test_modified()." << endl;

  // f1.rm();
  return err_count;
}

/*
  Test that we can tell if files and directories are writeable or not.
*/
int test_is_writeable(const string dir, const string base,
                      const bool result_expected) {
  File my_file(base, dir);
  if (my_file.is_writeable() == result_expected)
    return 0;
  string errstr(strerror(errno));
  cout << "test_is_writeable(" << dir << ", " << base << ", " << result_expected
       << ")" << endl;
  cout << "    [" << my_file.full_path() << "]" << endl;
  cout << "    [" << errstr << "]" << endl;
  return 1;
}

/*
  Test that we can tell if directories are writeable or not.
*/
int test_dir_is_writeable(const string dir, const bool result_expected) {
  File my_file(".", dir);
  if (my_file.dir_is_writeable() != my_file.is_writeable())
    return 1;
  if (my_file.dir_is_writeable() == result_expected)
    return 0;
  string errstr(strerror(errno));
  cout << "test_dir_is_writeable(" << dir << ", " << result_expected << ")"
       << endl;
  cout << "    [" << my_file.full_path() << "]" << endl;
  cout << "    [" << errstr << "]" << endl;
  return 1;
}
}

int main(int argc, char *argv[]) {
  cout << "Testing file.cpp" << endl;

  mode(Verbose, false);
  mode(Testing, true);
  mode(ReadOnly, false);

  int err_count = 0;
  File f_tmp("", "/tmp/");
  string contents("");
  f_tmp.file_contents(contents);
  err_count += test_exists(f_tmp.dirname(), f_tmp.basename(), true);
  f_tmp.rm();
  err_count += test_exists(f_tmp.dirname(), f_tmp.basename(), false);

  //  /bin/echo always exists on linux systems, right?
  err_count += test_exists("/bin", "echo", true);
  err_count += test_exists("/bin", "echo-not", false);

  vector_string messages = test_text();
  err_count += count_if(messages.begin(), messages.end(), test_file);

  err_count += test_modified();

  err_count += test_is_writeable("/tmp", ".", true);
  err_count += test_is_writeable("/var/log", ".",
                                 false); // or else your system is strange...
  err_count += test_is_writeable("/dev", "null", true);
  err_count += test_is_writeable("/var/log", "syslog", false);

  // or else your system is strange...
  err_count += test_dir_is_writeable("/tmp", true);
  err_count += test_dir_is_writeable("/dev", false);
  err_count += test_dir_is_writeable("/var/log", false);

  if (err_count)
    cout << "Errors (" << err_count << ") in test!!" << endl;
  else
    cout << "All tests passed!" << endl;
  return 0 != err_count;
}
