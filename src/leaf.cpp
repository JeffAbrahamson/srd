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

#include "srd.h"


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
Leaf::Leaf(const string &pass,
           const string base_name,
           const string dir_name,
           const bool do_load)
        : m_password(pass), m_modified(false), m_loaded(false)
{
        basename(base_name);    // If empty, will be computed for us
        dirname(dir_name);      // If empty, will be computed for us
        if(!exists()) {
                // If never persisted, then ok to return empty leaf.
                m_loaded = true;
                return;         
        }
        if(!do_load) {
                // Not loading imposes on the client an obligation not
                // to take actions that assume the contents are
                // loaded.  In particular, the client must not ask for
                // the key or the payload.  (We assert that this is
                // respected.)  Dynamic loading causes const problems,
                // the backflips for which seem unreasonable.
                /*
                if(mode(Verbose))
                        cout << "Initializing leaf without load." << endl;
                */
                return;
        }
        load();
        validate();
}



/*
  Save ourselves:
  Serialize, compress, encrypt, and persist to the file whose name we know
  from construction time.
*/
Leaf::~Leaf()
{
        commit();
}


/*
  Load ourselves from the file.

  It is possible that we reload leaves when we don't need to.  See the
  note at LeafProxy::operator=().  Brief, we're protecting against a
  harder problem of making sure a leaf can't be loaded twice and
  modified separately in two copies.  That problem isn't
  insurmountable, but neither is it currently important to fix in the
  context of srd.
*/
void Leaf::load()
{
        if(m_loaded)
                return;
        if(mode(Verbose))
                cout << "Loading leaf:  " << basename() << endl;
        string plain_text = decrypt(file_contents(), m_password);
        string big_text = decompress(plain_text);
        istringstream big_text_stream(big_text);
        boost::archive::text_iarchive ia(big_text_stream);
        ia & *this;
        m_loaded = true;
}



/*
  If we've been modified, then write to our file.
*/
void Leaf::commit()
{
        validate();
        if(!m_modified || mode(ReadOnly))
                return;
        ostringstream big_text_stream;
        boost::archive::text_oarchive oa(big_text_stream);
        oa & *this;
        string big_text(big_text_stream.str());
        string plain_text = compress(big_text);
        string cipher_text = encrypt(plain_text, m_password);
        file_contents(cipher_text);
        m_modified = false;
        validate();
        if(mode(Verbose))
                cout << "leaf committed" << endl;
}



/*
  Remove the underlying file.  We mark as unmodified so that we won't
  try to repersist at destruction time.
*/
void Leaf::erase()
{
        validate();
        rm();
        m_modified = false;
        validate();
}



/*
  Check that all is in order.  If not, assert and die.
  All should be in order!
*/
void Leaf::validate()
{
        assert(m_password.size() > 0);
        assert(basename().size() > 0);
        assert(dirname().size() > 0);
}



/*
  Serialize or deserialize according to context.
*/
template<class Archive>
void Leaf::serialize(Archive &ar, const unsigned int version)
{
        ar & m_node_key;
        ar & m_node_payload;
}



