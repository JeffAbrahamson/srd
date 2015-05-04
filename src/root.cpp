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

#include "srd.h"


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
Root::Root(const string &pass, const string dir_name, const bool create)
    : modified(false), password(pass), valid(true)
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
    if(!is_writeable())
	mode(ReadOnly, true);
    if(!exists()) {
	if(mode(ReadOnly)) {
	    cerr << "Refusing to create new root node while read-only." << endl;
	    throw("No persistence permitted while read-only.");
	}
	cout << "Root node does not exist, will create." << endl;
	if(mode(Verbose))
	    cout << "    [" << full_path() << "]" << endl;
	modified = true;
	validate();
	return;
    }
    load();
    validate();
}



/*
  Load the root's contents.
*/
void Root::load()
{
    if(modified) {
	cerr << "Uncommitted change to root and external change to root file.  "
	    "Data will be lost." << endl;
	throw(runtime_error("Refusing to modify externally modified root."));
    }
    if(mode(Verbose))
	cout << "Loading root:  " << basename() << endl;
    if(!mode(ReadOnly) && (!is_writeable() || !dir_is_writeable())) {
	cout << "Opening database in read-only mode." << endl;
	mode(ReadOnly, true);
    }
    clear();                // Drop existing LeafProxy's, if any
    string plain_text;
    {
	Lock L(full_path() + ".lck");
	plain_text = decrypt(file_contents(), password);
    }
    string big_text = decompress(plain_text);
    RootData root_data;
    if(!root_data.ParseFromString(big_text)) {
	if(!getenv("SRD_TEST_PROTOBUF_ONLY")) {
	    // Assume that if we fail to deserialize, then it's the old format.
	    // Unless we are testing in preparation to abandon the old format.
	    istringstream big_text_stream(big_text);
	    boost::archive::text_iarchive ia(big_text_stream);
	    ia & *this;

	    for_each(leaf_names.begin(),
		     leaf_names.end(),
		     boost::bind(&Root::instantiate_leaf_proxy, this, _1));
	    assert(size() == leaf_names.size());
	    leaf_names.clear();
	    validate();
	    return;
	}
	// When we retire the old Boost format, we'll throw an error
	// instead of the above block.  We'll also be able to delete
	// class LeafProxyPersit.
	cerr << "Failed to deserialize root." << endl;
	throw(runtime_error("Failed to deserialize root"));
    }
    // TODO(jeff@purple.com): It's a kludge to capture this in the lambda closure.
    for_each(root_data.keys().begin(),
	     root_data.keys().end(),
	     [root_data, this](RootData_KeyData key) mutable {
		 (*this)[key.proxy_name()] = LeafProxy(password, key.proxy_name(), "");
		 (*this)[key.proxy_name()].key_cache(key.cached_key());
	     });
    assert(size() == root_data.keys_size());
    validate();
}



/*
  Kludge.  In order to persist the keys of the leaf_proxy_map leaves,
  we copy them to a vector and persist that instead.  Here we
  reinstantiate the map from the vector elements.
*/
void Root::instantiate_leaf_proxy(LeafProxyPersist &proxy_info)
{
    (*this)[proxy_info.proxy_name] = LeafProxy(password, proxy_info.proxy_name, "");
    (*this)[proxy_info.proxy_name].key_cache(proxy_info.cached_key);
}



/*
  Kludge.  In order to persist the keys of the leaf_proxy_map leaves,
  we copy them to a vector and persist that instead.
*/
void Root::populate_leaf_names(LeafProxyMap::value_type &val)
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
void Root::add_leaf(const string &key, const string &payload, const bool do_commit)
{
    validate();
    if(exists() && underlying_is_modified())
	load();
    LeafProxy proxy(password, "", dirname());
    proxy.set(key, payload);
    (*this)[proxy.basename()] = proxy;
    modified = true;        // Adding a leaf requires persisting the root.
    if(do_commit)
	// Best practice is to commit, and so do_commit
	// defaults to true.  If the client knows that it will
	// add quite a few things in short order, it might
	// take the shortcut of delaying commit (and so
	// explicitly calling commit()).  The root will in any
	// case be committed if necessary at destruction.
	commit();
    validate();
}



/*
  Return the leaf_proxy object with the given proxy key.
  It is an error for the object not to exist.
*/
LeafProxy Root::get_leaf(const string &proxy_key)
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
void Root::set_leaf(const string &proxy_key,
                    const string &key,
                    const string &payload)
{
    validate();
    if(exists() && underlying_is_modified())
	load();
    iterator it = find(proxy_key);
    if(end() == it)
	throw(runtime_error("Key not found."));
    LeafProxy &proxy = it->second;
    if(proxy.set(key, payload))
	modified = true;
    validate();
}



/*
  Remove a leaf_proxy from the root, deleting the underlying leaf
  file.
*/
void Root::rm_leaf(const string &proxy_key)
{
    validate();
    if(exists() && underlying_is_modified())
	load();
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
Root Root::change_password(const std::string &new_password)
{
    validate();
    if(exists() && underlying_is_modified())
	load();
    Root new_root(new_password, dirname(), true);
    for(const_iterator it = begin();
	it != end();
	it++)
	new_root.add_leaf((*it).second.key(), (*it).second.payload(), false);
    new_root.commit();
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
    if(!modified || mode(ReadOnly))
	return;

    RootData root_data;
    for_each(begin(),
	     end(),
	     [&root_data](LeafProxyMapInternalType::value_type val) mutable {
		 RootData_KeyData* key_data = root_data.add_keys();
		 key_data->set_proxy_name(val.first);
		 key_data->set_cached_key(val.second.key());
	     });
    string big_text;
    if(!root_data.SerializeToString(&big_text)) {
	cerr << "Failed to serialize root." << endl;
	throw(runtime_error("Failed to serialize root."));
    }
    string plain_text = compress(big_text);
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

  If force_load is true, we load every leaf to make sure it really
  does load and is consistent.
*/
void Root::validate(bool force_load) const
{
    assert(valid);
    assert(password.size() > 0);
    // Validate each of the leaf proxies.  Note that this won't cause them to load.
    for(const_iterator it = begin(); it != end(); it++) {
	assert((*it).first == (*it).second.basename());
	(*it).second.validate(force_load);
    }
    assert(leaf_names.size() == 0);
}



/*
  Checksum all keys and payloads together.
*/
void Root::checksum(bool force_load) const
{
    validate(true);
    LeafProxyMap::LPM_Set leaves = this->as_set();
    string text;
    for(LeafProxyMap::LPM_Set::iterator it = leaves.begin();
	it != leaves.end();
	++it) {
	string key = (*it).key();
	string value = (*it).payload();
	text.append(key);
	text.append(value);
    }
    cout << message_digest(text) << endl;
}



template<class Archive>
void Root::serialize(Archive &ar, const unsigned int version)
{
    ar & leaf_names;
}
