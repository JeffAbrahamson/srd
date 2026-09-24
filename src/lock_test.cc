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

#include "srd.h"

#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace srd;
using namespace std;

namespace {

int err_count = 0;

void check(bool ok, const string &what) {
  if (!ok) {
    cout << "Failed:  " << what << endl;
    err_count++;
  }
}

/*
  Return true if another process could take the lock in dir right now.
  Must be another process:  flock() locks belong to open file
  descriptions, so a probe from this process would say nothing about
  what other processes see.
*/
bool lock_is_free(const string &dir) {
  pid_t pid = fork();
  if (0 == pid) {
    int fd = open((dir + "/lock").c_str(), O_RDONLY);
    _exit(fd >= 0 && 0 == flock(fd, LOCK_EX | LOCK_NB) ? 0 : 1);
  }
  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && 0 == WEXITSTATUS(status);
}

} // namespace

int main() {
  cout << "Testing lock.cc" << endl;

  mode(Verbose, false);
  mode(Testing, true);
  mode(ReadOnly, false);

  char dir_template[] = "/tmp/srd-lock-test-XXXXXX";
  string dir = mkdtemp(dir_template);

  {
    Lock outer(dir);
    check(!lock_is_free(dir), "lock excludes other processes");
    {
      // Same directory by another path:  must not deadlock.
      Lock inner(dir + "/.");
      check(!lock_is_free(dir), "re-entrant lock held");
    }
    check(!lock_is_free(dir), "lock still held after inner release");
  }
  check(lock_is_free(dir), "lock released");
  check(filesystem::exists(dir + "/lock"), "lock file persists");

  mode(ReadOnly, true);
  {
    Lock read_only(dir);
    check(!lock_is_free(dir), "existing lock respected when read-only");
  }
  check(lock_is_free(dir), "read-only lock released");
  char empty_template[] = "/tmp/srd-lock-test-XXXXXX";
  string empty_dir = mkdtemp(empty_template);
  {
    Lock read_only(empty_dir);
    check(!filesystem::exists(empty_dir + "/lock"),
          "no lock file created when read-only");
  }
  filesystem::remove(empty_dir);

  filesystem::remove_all(dir);
  if (err_count)
    cout << "Errors (" << err_count << ") in test!!" << endl;
  else
    cout << "All tests passed!" << endl;
  return 0 != err_count;
}
