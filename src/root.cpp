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
Root::Root(const string pass, const string dir_name, const bool create)
        : password(pass), modified(false), valid(true)
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
                 boost::bind(&Root::instantiate_leaf_proxy, this, _1));
        assert(size() == leaf_names.size());
        leaf_names.clear();
        validate();
}



/*
  Kludge.  In order to persist the keys of the leaf_proxy_map leaves,
  we copy them to a vector and persist that instead.  Here we
  reinstantiate the map from the vector elements.
*/
void Root::instantiate_leaf_proxy(LeafProxyPersist proxy_info)
{
        (*this)[proxy_info.proxy_name] = LeafProxy(password, proxy_info.proxy_name, "");
        (*this)[proxy_info.proxy_name].key_cache(proxy_info.cached_key);
}



/*
  Kludge.  In order to persist the keys of the leaf_proxy_map leaves,
  we copy them to a vector and persist that instead.
*/
void Root::populate_leaf_names(LeafProxyMap::value_type val)
{
        LeafProxyPersist lpp;
        lpp.proxy_name = val.first;
        lpp.cached_key = val.second.key();
        leaf_names.push_back(lpp);
}





/*
  Serialize and persist the node before destruction.
*/
Root::~Root()
{
        if(valid) {
                validate();
                commit();
        }
}



/*
  Add a leaf with key and payload.
  Insert the proxy key and leaf_proxy into the root.
*/
void Root::add_leaf(const string key, const string payload)
{
        validate();
        LeafProxy proxy(password, "", dirname());
        proxy.set(key, payload);
        (*this)[proxy.basename()] = proxy;
        modified = true;        // Adding a leaf requires persisting the root.
        commit();
        validate();
}



/*
  Return the leaf_proxy object with the given proxy key.
  It is an error for the object not to exist.
*/
LeafProxy Root::get_leaf(const string proxy_key)
{
        validate();
        iterator it = find(proxy_key);
        if(end() == it)
                throw(runtime_error("Key not found."));
        return it->second;
}



/*
  Set the contents of an existing leaf.
  To create a new leaf, use add_leaf().
*/
void Root::set_leaf(const string proxy_key,
                    const string key,
                    const string payload)
{
        validate();
        iterator it = find(proxy_key);
        if(end() == it)
                throw(runtime_error("Key not found."));
        LeafProxy &proxy = it->second;
        proxy.set(key, payload);
        validate();
}



/*
  Remove a leaf_proxy from the root, deleting the underlying leaf
  file.
*/
void Root::rm_leaf(const string proxy_key)
{
        validate();
        iterator it = find(proxy_key);
        if(end() == it)
                throw(runtime_error("Key not found."));
        it->second.erase();
        erase(it);
        modified = true;
        commit();
        validate();
}



/*
  Change the password associated with this root and all its leaves.

  First make a new root and, for each leaf in the old root, add it to
  the new root.  Once the new root is complete and persisted, walk the
  old root and remove its leaves (and so the underlying files).
  Finally, remove the old root and mark it invalid so that any further
  operations on it will fail.

  Return the new root.  Will throw runtime_error if the new password
  generates an existing root object.
*/
Root Root::change_password(const std::string new_password)
{
        validate();
        Root new_root(new_password, dirname(), true);
        for(const_iterator it = begin();
            it != end();
            it++)
                new_root.add_leaf((*it).second.key(), (*it).second.payload());
        while(!empty()) {
                iterator it = begin();
                (*it).second.erase();
                erase(it);
        }
        validate();
        valid = false;
        new_root.validate();
        return new_root;
}



/*
  If we have been modified, persist to our underlying file.
*/
void Root::commit()
{
        validate();
        if(!modified)
                return;
        for_each(begin(),
                 end(),
                 boost::bind(&Root::populate_leaf_names, this, _1));
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
void Root::validate()
{
        assert(valid);
        assert(password.size() > 0);
        // Validate each of the leaf proxies.  Note that this won't cause them to load.
        for(iterator it = begin(); it != end(); it++) {
                assert((*it).first == (*it).second.basename());
                (*it).second.validate();
        }
        assert(leaf_names.size() == 0);
}



template<class Archive>
void Root::serialize(Archive &ar, const unsigned int version)
{
        ar & leaf_names;
}



