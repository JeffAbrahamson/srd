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



#ifndef __FILE_H__
#define __FILE_H__ 1


#include <boost/interprocess/sync/file_lock.hpp>
#include <string>


namespace srd {

        /*
          A simple mix-in class to handle reading and writing (binary) files,
          as well as testing for existence and removing them.

          This could surely be done more cleverly.  Or already has been.

          At construction time, we can optionally specify a directory and a name
          (base_name) for the file.  Otherwise, these are determined following
          policy for the srd application.  
        */
        class File {
                
        public:
                File(const std::string base_name = std::string(),
                     const std::string dir_name = std::string());
                ~File() { if(m_lock) delete m_lock; };

                std::string dirname();
                void dirname(const std::string in) { m_dir_name = in; }
                
                std::string basename();
                void basename(const std::string in) { m_base_name = in; }
                
                std::string full_path() { return dirname() + "/" + basename(); }
                
                void file_contents(std::string);
                std::string file_contents();

                time_t modtime(const bool silent = true);
                bool underlying_is_modified();
                void lock();
                void unlock();
                
                void rm();
                bool exists();

                
        protected:

                time_t m_time(const bool silent = true);
                

        private:

                std::string m_dir_name;
                std::string m_base_name;
                
                // Once true, we no longer check to see if the directory exists.
                // If false, we'll check and create if needed.
                bool m_dir_verified;

                // When first we read the file, check it's mod time.
                // If that changes. we'll know to reread.
                time_t m_modtime;

                boost::interprocess::file_lock *m_lock;
        };
}

#endif  /* __FILE_H__*/
