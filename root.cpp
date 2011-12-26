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
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/bind.hpp>
#include <boost/serialization/vector.hpp>
#include <functional>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <unistd.h>

#include "compress.h"
#include "crypt.h"
#include "mode.h"
#include "file.h"
#include "root.h"
#include "leaf_proxy.h"


using namespace srd;
using namespace std;



/*
  Instantiate a root node.
  If provided, path is the directory in which to find the srd encrypted files.

  The argument dir_name is only useful if the app supports pointing to
  an alternate directory.  Otherwise, it should be an empty string so
  that the directory may be computed based on app (or library) policy.

  The name of the root node must be determinable solely by the password.
*/
root::root(const string pass, const string dir_name, const bool create)
        : password(pass), modified(false)
{
        string base_name(pass);
        for(int i = 0; i < 30; i++)
                base_name = message_digest(base_name, true);
        basename(base_name);    // Must be reproducible from password alone
        dirname(dir_name);      // If empty, will be computed for us
        if(exists() == create) {
                // i.e., if exists() != !create
                if(create)
                        throw(runtime_error("Can't create existing root."));
                throw(runtime_error("Root doesn't exist.  (Incorrect password?)"));
        }
        if(!exists()) {
                cout << "Root node does not exist, will create." << endl;
                if(mode(Verbose))
                        cout << "    [" << full_path() << "]" << endl;
                modified = true;
                validate();
                return;
        }
        // Now load the contents.  This is the only place we consider
        // loading the root node's contents.
        string plain_text = decrypt(file_contents(), password);
        string big_text = decompression(plain_text);
        istringstream big_text_stream(big_text);
        boost::archive::text_iarchive ia(big_text_stream);
        ia & *this;

        for_each(leaf_names.begin(),
                 leaf_names.end(),
                 boost::bind(&root::instantiate_leaf_proxy, this, _1));
        assert(size() == leaf_names.size());
        leaf_names.clear();
        validate();
}



/*
  Kludge.  In order to persist the keys of the leaf_proxy_map leaves,
  we copy them to a vector and persist that instead.  Here we
  reinstantiate the map from the vector elements.
*/
void root::instantiate_leaf_proxy(leaf_proxy_persist proxy_info)
{
        (*this)[proxy_info.proxy_name] = leaf_proxy(password, proxy_info.proxy_name, "");
        (*this)[proxy_info.proxy_name].key_cache(proxy_info.cached_key);
}



/*
  Kludge.  In order to persist the keys of the leaf_proxy_map leaves,
  we copy them to a vector and persist that instead.
*/
void root::populate_leaf_names(leaf_proxy_map::value_type val)
{
        leaf_proxy_persist lpp;
        lpp.proxy_name = val.first;
        lpp.cached_key = val.second.key();
        leaf_names.push_back(lpp);
}





/*
  Serialize and persist the node before destruction.
*/
root::~root()
{
        validate();
        commit();
}



/*
  Add a leaf with key and payload.
  Insert the proxy key and leaf_proxy into the root.
*/
void root::add_leaf(const string key, const string payload)
{
        validate();
        leaf_proxy proxy(password, "", dirname());
        proxy.key(key);
        proxy.payload(payload);
        proxy.commit();
        (*this)[proxy.basename()] = proxy;
        modified = true;        // Adding a leaf requires persisting the root.
        validate();
}



/*
  Return the leaf_proxy object with the given proxy key.
  It is an error for the object not to exist.
*/
leaf_proxy root::get_leaf(const string proxy_key)
{
        iterator it = find(proxy_key);
        if(end() == it)
                throw(runtime_error("Key not found."));
        return it->second;
}



void root::set_leaf(const string proxy_key,
                    const string key,
                    const string payload)
{
        iterator it = find(proxy_key);
        if(end() == it)
                throw(runtime_error("Key not found."));
        leaf_proxy &proxy = it->second;
        proxy.key(key);
        proxy.payload(payload);
        proxy.commit();
}



void root::rm_leaf(const string proxy_key)
{
        iterator it = find(proxy_key);
        if(end() == it)
                throw(runtime_error("Key not found."));
        it->second.erase();
        erase(it);
}



/*
  If we have been modified, persist to our underlying file.
*/
void root::commit()
{
        validate();
        if(!modified)
                return;
        for_each(begin(),
                 end(),
                 boost::bind(&root::populate_leaf_names, this, _1));
        assert(size() == leaf_names.size());

        ostringstream big_text_stream;
        boost::archive::text_oarchive oa(big_text_stream);
        oa & *this;
        string big_text(big_text_stream.str());
        string plain_text = compression(big_text);
        string cipher_text = encrypt(plain_text, password);
        file_contents(cipher_text);

        leaf_names.clear();
        validate();
        if(mode(Verbose))
                cout << "root committed, size=" << size() << endl;
}



/*
  Confirm that all is well.
  It is an error if all is not, and we will die.
*/
void root::validate()
{
        assert(password.size() > 0);
        // Validate each of the leaf proxies.  Note that this won't cause them to load.
        for(iterator it = begin(); it != end(); it++) {
                assert((*it).first == (*it).second.basename());
                (*it).second.validate();
        }
        assert(leaf_names.size() == 0);
}



template<class Archive>
void root::serialize(Archive &ar, const unsigned int version)
{
        ar & leaf_names;
}



