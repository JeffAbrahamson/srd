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
#include <functional>
#include <string>

#include "leaf_proxy.h"


using namespace srd;
using namespace std;



/*
  We need this just for map::operator[]().

  So allow the object to be created, but flag it as invalid, which
  only a proper assignment or copy will cure.
*/
leaf_proxy::leaf_proxy()
{
        the_leaf = NULL;
        valid = false;          // check that we've followed on with an assignment or copy
}



/*
  Create a leaf proxy properly, unlike the no-argument constructor.
  The filename (base) and directory (dir) may be empty and will be
  created if needed.
*/
leaf_proxy::leaf_proxy(const string pass,
                       const string base,
                       const string dir,
                       const bool test)
        : password(pass), base_name(base), dir_name(dir), testing(test)
{
        assert(!password.empty());
        the_leaf = NULL;
        valid = true;
        validate();
}



/*
  Copy constructor.  Note that we don't copy the leaf we proxy, just
  the information to be able to proxy it.  See the comment in
  operator=() about why.
*/
leaf_proxy::leaf_proxy(const leaf_proxy &other)
        : password(other.password),
          base_name(other.base_name),
          dir_name(other.dir_name),
          testing(other.testing),
          valid(other.valid),
          the_leaf(NULL)
{
        validate();
}



/*
  Self-assignment is fine, but we'll lose the_leaf.  But we always
  lose the object we proxy on assignment.  If we need it, we'll just
  read it in again.  This avoids copying a leaf with modifications
  pending, then one is canceled or has erase() called on it and the
  other gets destroyed and so writes itself.
*/
leaf_proxy &leaf_proxy::operator=(const leaf_proxy &other)
{
        password = other.password;
        base_name = other.base_name;
        dir_name = other.dir_name;
        testing = other.testing;
        valid = other.valid;
        the_leaf = NULL;

        validate();
        return *this;
}



/*
  Set the leaf's key.
*/
void leaf_proxy::key(const string in)
{
        init_leaf();
        the_leaf->key(in);
        validate();
}



/*
  Return the leaf's key.
*/
const string leaf_proxy::key()
{
        init_leaf();
        validate();
        return the_leaf->key();
}



/*
  Set the leaf's payload.
*/
void leaf_proxy::payload(const string in)
{
        init_leaf();
        the_leaf->payload(in);
        validate();
}



/*
  Return the leaf's payload.
*/
const string leaf_proxy::payload()
{
        init_leaf();
        validate();
        return the_leaf->payload();
}



/*
  Return the name of the leaf's file.
  The only real reason for this is that the root will need a name
  by which to instantiate leaves.
*/
const string leaf_proxy::basename()
{
        validate();
        init_leaf();
        return the_leaf->basename();
}



/*
  Commit any changes to the leaf.
  If we haven't loaded a leaf, just return without doing anything.
*/
void leaf_proxy::commit()
{
        validate();
        if(the_leaf) {
                the_leaf->commit();
                validate();
        }
}



/*
  Remove the underlying leaf's file and unload the leaf.  In the
  process, load the leaf if we need to.  If we later try to read from
  the leaf, we'll get an error, but writing will succeed (and will
  recreate the leaf).
*/
void leaf_proxy::erase()
{
        validate();
        init_leaf();
        the_leaf->erase();
        the_leaf = NULL;
        validate();
}



/*
  Load the leaf if its file exists.  Otherwise just initialize the leaf.
*/
void leaf_proxy::init_leaf()
{
        validate();
        if(!the_leaf)
                the_leaf = new leaf(password, base_name, dir_name, testing);
        if(!base_name.empty())
                assert(base_name == the_leaf->basename());
        if(!dir_name.empty())
                assert(dir_name == the_leaf->dirname());
        base_name = the_leaf->basename();
        dir_name = the_leaf->dirname();
        validate();
}



/*
  Confirm that all is well.
*/
void leaf_proxy::validate() const
{
        if(!the_leaf)
                return;
        assert(valid);
        assert((void *)the_leaf > (void *)0x1FF); // kludge, catch some bad pointers
        assert(base_name == the_leaf->basename());
        assert(dir_name == the_leaf->dirname());
        the_leaf->validate();
}



////////////////////////////////////////////////////////////////////////////////
//
//   leaf_matcher
//
////////////////////////////////////////////////////////////////////////////////


leaf_matcher::leaf_matcher(const vector_string in_key,
                           const vector_string in_payload,
                           bool conj)
        : key(in_key), payload(in_payload), conjunction(conj)
{
}



/*
  Because I can't get
  mem_fun(&string::find)
  to work for me today.
*/
class find_in_string {
public:
        find_in_string(string s) : str(s) {};
        bool operator()(string s) { return str.find(s) != string::npos; };
private:
        string str;
};



/*
  Return true if the leaf_proxy matches our criteria.

  A successful match has all key patterns matching the key
  and/or (depending on whether conjunction is true/false)
  all payload patterns matching the payload.
*/
bool leaf_matcher::operator()(leaf_proxy &proxy)
{
        if(key.size() == 0 && payload.size() == 0)
                return true;
        string the_key = proxy.key();
        unsigned int key_match_count = count_if(key.begin(),
                                                key.end(),
                                                find_in_string(the_key));
        bool all_keys_found = (key_match_count == key.size());
        if(all_keys_found && conjunction == false)
                return true;
        if(!all_keys_found && conjunction == true)
                return false;
                
        string the_payload = proxy.payload();
        unsigned int payload_match_count = count_if(payload.begin(),
                                                    payload.end(),
                                                    find_in_string(the_payload));
        bool all_payloads_found = (payload_match_count == payload.size());
        return all_payloads_found;
}


