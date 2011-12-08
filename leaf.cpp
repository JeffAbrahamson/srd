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
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <string>
#include <sstream>

#include "crypt.h"
#include "leaf.h"
#include "mode.h"


using namespace srd;
using namespace std;



/*
  If newly created, just decide what our filename is and we're done.
  If we know our filename, then fetch the file contents, decrypt, decompress,
  and deserialize to member variables.

  We don't have a concept of a leaf with persisted but unloaded data.
  That concept lives in the leaf_proxy class, which creates or destroys
  a leaf object as it sees fit.

  If load == false, we don't actually load the leaf's contents.
  Don't do this.  It's just a hook for removing leaves that we
  haven't loaded.  The only usage should be leaf_proxy::erase().
*/
leaf::leaf(const string pass,
           const string base_name,
           const string dir_name,
           const bool load)
        : password(pass), modified(false)
{
        basename(base_name);    // If empty, will be computed for us
        dirname(dir_name);      // If empty, will be computed for us
        if(!exists())
                return;         // If never persisted, then ok to return empty leaf
        if(!load) {
                if(mode(Verbose))
                        cout << "Initializing leaf without load." << endl;
                return;
        }
        if(mode(Verbose))
                cout << "Loading leaf." << endl;
        string plain_text = decrypt(file_contents(), password);
        string big_text = decompression(plain_text);
        istringstream big_text_stream(big_text);
        boost::archive::text_iarchive ia(big_text_stream);
        ia & *this;
        validate();
}



/*
  Save ourselves:
  Serialize, compress, encrypt, and persist to the file whose name we know
  from construction time.
*/
leaf::~leaf()
{
        commit();
}



/*
  If we've been modified, then write to our file.
*/
void leaf::commit()
{
        validate();
        if(!modified)
                return;
        ostringstream big_text_stream;
        boost::archive::text_oarchive oa(big_text_stream);
        oa & *this;
        string big_text(big_text_stream.str());
        string plain_text = compression(big_text);
        string cipher_text = encrypt(plain_text, password);
        file_contents(cipher_text);
        modified = false;
        validate();
        if(mode(Verbose))
                cout << "leaf committed" << endl;
}



/*
  Remove the underlying file.  We mark as unmodified so that we won't
  try to repersist at destruction time.
*/
void leaf::erase()
{
        validate();
        rm();
        modified = false;
        validate();
}



/*
  Check that all is in order.  If not, assert and die.
  All should be in order!
*/
void leaf::validate()
{
        assert(password.size() > 0);
        assert(basename().size() > 0);
        assert(dirname().size() > 0);
}



/*
  Serialize or deserialize according to context.
*/
template<class Archive>
void leaf::serialize(Archive &ar, const unsigned int version)
{
        ar & node_key;
        ar & node_payload;
}



