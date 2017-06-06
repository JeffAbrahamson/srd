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

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <string>

#include "srd.h"

using namespace boost::interprocess;
using namespace boost::posix_time;
using namespace srd;
using namespace std;

/*
  RAII lock.
*/
Lock::Lock(const string &filename)
    : m_filename(filename), m_must_remove(false), m_lock(0) {
  if (mode(ReadOnly)) {
    if (mode(Verbose))
      cout << "Skipping lock while read-only" << endl;
    return;
  }
  // This will likely fail for all but regular files.
  // It will also fail if we can't write the file.
  if (!file_exists(m_filename)) {
    // Can't lock on what doesn't exist
    file_create(m_filename);
    m_must_remove = true;
  }
  try {
    m_lock = new file_lock(m_filename.c_str());
  } catch (interprocess_exception &e) {
    // It should be ok run against a read-only filesystem
    const char *what = e.what();
    if (strcmp(what, "Permission denied") == 0) {
    } else
      (throw e);
  }

  const int wait_delay = 2; // seconds in the future to try again on fail
  boost::posix_time::ptime timeout = from_time_t(time(0) + wait_delay);
  while (!m_lock->timed_lock(timeout)) {
    cout << "Waiting on file lock..." << endl;
    timeout = from_time_t(time(0) + 1);
  }
}

Lock::~Lock() {
  if (m_lock) {
    // With delete immediately after unlock(), is
    // unlock() necessary?  It's not entirely clear from
    // the code just now, so leave it here for the
    // moment.
    m_lock->unlock();
    delete m_lock;
  }
  if (m_must_remove)
    file_rm(m_filename);
}
