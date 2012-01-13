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



#ifndef __LOCKER_H__
#define __LOCKER_H__ 1

#include <boost/interprocess/sync/file_lock.hpp>
#include <string>


namespace srd {

        class Lock {

        public:
                
                Lock(const std::string &filename);
                ~Lock();

                
        private:
                
                std::string m_filename; /* Name of the lock file */
                bool m_must_remove;
                boost::interprocess::file_lock *m_lock;
        };

}

#endif  /* __LOCKER_H__*/
