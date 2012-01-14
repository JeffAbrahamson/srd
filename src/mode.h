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



#ifndef __MODE_H__
#define __MODE_H__ 1


namespace srd {
        
        /*
          Maintain a map of modes.

          Written for verbose and testing, to avoid having to pass them around
          everywhere in case we need them deep down.  Especially verbose.
  
          Some might call this a kludge or even inelegant.  Abused, it would be.
        */


        enum Mode {
                Verbose,
                Testing,
        };

        void mode(const Mode m, const bool new_state);
        const bool mode(const Mode m);

}

#endif  /* __MODE_H__*/
