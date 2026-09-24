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

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "srd.h"

using namespace srd;
using namespace std;

namespace {

// Locks this process holds:  the open descriptor (closing it releases
// the lock) and how many Lock objects share it.  Keyed by the lock
// file's identity, not its name, since one directory may be reached
// by different paths, and a second flock() on the same file from
// this process would wait forever on the first.
struct HeldLock {
  int fd;
  int depth;
};
map<pair<dev_t, ino_t>, HeldLock> held_locks;

} // namespace

/*
  Take the database lock, waiting for it if another process has it.

  The lock file is never removed.  Removing a lock file on unlock
  breaks mutual exclusion:  a process already waiting holds a lock on
  the removed file, while a newcomer creates and locks a new one.
*/
Lock::Lock(const string &dir_name)
    : m_filename(dir_name + "/lock"), m_held(false) {
  // O_CLOEXEC, so that an editor we start doesn't inherit the lock
  // and keep holding it should we die first.  flock() works on a
  // read-only descriptor too.
  int fd;
  if (mode(ReadOnly)) {
    // Don't create anything, but do respect a lock that exists.
    fd = open(m_filename.c_str(), O_RDONLY | O_CLOEXEC);
    if (-1 == fd) {
      if (mode(Verbose))
        cout << "Skipping lock while read-only" << endl;
      return;
    }
  } else {
    fd = open(m_filename.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (-1 == fd && (EACCES == errno || EROFS == errno))
      fd = open(m_filename.c_str(), O_RDONLY | O_CLOEXEC);
    if (-1 == fd)
      throw(runtime_error("Failed to open lock file " + m_filename + ":  " +
                          strerror(errno)));
  }
  struct stat st;
  if (fstat(fd, &st)) {
    string err = strerror(errno);
    close(fd);
    throw(runtime_error("Failed to stat " + m_filename + ":  " + err));
  }
  m_id = {st.st_dev, st.st_ino};
  auto it = held_locks.find(m_id);
  if (held_locks.end() != it) {
    close(fd);
    ++it->second.depth;
    m_held = true;
    return;
  }

  if (flock(fd, LOCK_EX | LOCK_NB)) {
    if (EWOULDBLOCK != errno) {
      string err = strerror(errno);
      close(fd);
      throw(runtime_error("Failed to lock " + m_filename + ":  " + err));
    }
    cerr << "Waiting for another srd to finish with the database..." << endl;
    while (flock(fd, LOCK_EX)) {
      if (EINTR != errno) {
        string err = strerror(errno);
        close(fd);
        throw(runtime_error("Failed to lock " + m_filename + ":  " + err));
      }
    }
  }
  held_locks[m_id] = HeldLock{fd, 1};
  m_held = true;
}

Lock::~Lock() {
  if (!m_held)
    return;
  auto it = held_locks.find(m_id);
  if (0 == --it->second.depth) {
    close(it->second.fd); // Releases the lock.
    held_locks.erase(it);
  }
}
