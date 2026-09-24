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
#include <iostream>
#include <functional>
#include <sstream>
#include <string>

#include "srd.h"

using namespace boost;
using namespace srd;
using namespace std;

/*
  We need this just for map::operator[]().

  So allow the object to be created, but flag it as invalid, which
  only a proper assignment or copy will cure.
*/
LeafProxy::LeafProxy() {
  the_leaf = NULL;
  valid = false; // check that we've followed on with an assignment or copy
}

/*
  Create a leaf proxy properly, unlike the no-argument constructor.
  The filename (base) and directory (dir) may be empty and will be
  created if needed.
*/
LeafProxy::LeafProxy(const string &pass, const string base, const string dir)
    : password(pass), input_base_name(base), input_dir_name(dir) {
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
LeafProxy::LeafProxy(const LeafProxy &other)
    : password(other.password), input_base_name(other.input_base_name),
      input_dir_name(other.input_dir_name), valid(other.valid),
      cached_key(other.cached_key), the_leaf(NULL) {
  validate();
}

/*
  Self-assignment is fine, but we'll lose the_leaf.  But we always
  lose the object we proxy on assignment.  If we need it, we'll just
  read it in again.  This avoids copying a leaf with modifications
  pending, then one is canceled or has erase() called on it and the
  other gets destroyed and so writes itself.
*/
LeafProxy &LeafProxy::operator=(const LeafProxy &other) {
  // Read everything from other before releasing our own leaf, which
  // is other's if this is self-assignment.
  string new_password = other.password;
  string new_base_name, new_dir_name;
  if (other.the_leaf) {
    // If we've loaded the leaf, use its location
    // information, since it may have been computed at
    // instantiation time.
    new_base_name = other.the_leaf->basename();
    new_dir_name = other.the_leaf->dirname();
  } else {
    // Otherwise, what we have will surely work.
    new_base_name = other.input_base_name;
    new_dir_name = other.input_dir_name;
  }
  bool new_valid = other.valid;
  string new_cached_key = other.cached_key;

  // Release (and so commit) the leaf we had, as destruction would.
  delete_leaf();
  password = new_password;
  input_base_name = new_base_name;
  input_dir_name = new_dir_name;
  valid = new_valid;
  cached_key = new_cached_key;

  validate();
  return *this;
}

/*
  Set the key and payload.

  This is more efficient than subsequent calls to key(string) and
  payload(string), since it does a single commit on the leaf.

  Return whether or not the key has changed so that the client can
  take action if needed.
*/
bool LeafProxy::set(const string &in_key, const string &in_payload) {
  init_leaf();
  the_leaf->key(in_key);
  the_leaf->payload(in_payload);
  bool key_changed = (cached_key != in_key);
  cached_key = in_key;
  commit();
  validate();
  return key_changed;
}

/*
  Set the leaf's key.

  If setting the payload at the same time, using set() is more efficient,
  since it only does one commit on the underlying leaf.
*/
void LeafProxy::key(const string &in_key) {
  validate();
  init_leaf();
  the_leaf->key(in_key);
  cached_key = in_key;
  commit();
  validate();
}

/*
  Return the leaf's key.
*/
string LeafProxy::key() const {
  validate();
  if (cached_key.empty()) {
    // Trust not an empty cached_key
    init_leaf();
    return the_leaf->key();
  }
  return cached_key;
}

/*
  Set the leaf's payload.

  If setting the key at the same time, using set() is more efficient,
  since it only does one commit on the underlying leaf.
*/
void LeafProxy::payload(const string &in_payload) {
  validate();
  init_leaf();
  the_leaf->payload(in_payload);
  commit();
  validate();
}

/*
  Return the leaf's payload.
*/
string LeafProxy::payload() const {
  validate();
  init_leaf();
  validate();
  return the_leaf->payload();
}

/*
  Print the leaf's key.
*/
void LeafProxy::print_key() const {
  validate();
  cout << "[" << key() << "]" << endl;
}

/*
  Print the leaf's payload.
  Optionally filter the output for lines matching pattern.
*/
void LeafProxy::print_payload(const string &pattern) const {
  validate();
  string prefix = "  "; // Someday make this an option maybe
  stringstream payload_ss(payload());
  string line;
  while (getline(payload_ss, line, '\n'))
    if (0 == pattern.size() || string::npos != line.find(pattern))
      cout << prefix << line << endl;
}

/*
  Return the name of the leaf's file.
  The only real reason for this is that the root will need a name
  by which to instantiate leaves.
*/
string LeafProxy::basename() const {
  validate();
  // If the leaf is loaded, we can return the basename.  If not, and
  // we were given the name, return that:  Root::validate() asks for
  // every record's name, and building a Leaf costs a stat() or more.
  // Otherwise, don't load just to compute the name.  Rather, init
  // without loading, compute the name, and then toss the pointer.
  if (the_leaf)
    return the_leaf->basename();
  if (!input_base_name.empty())
    return input_base_name;
  init_leaf(false);
  string out_base_name(the_leaf->basename());
  delete_leaf();
  return out_base_name;
}

/*
  Commit any changes to the leaf.
  If we haven't loaded a leaf, just return without doing anything.
*/
void LeafProxy::commit() {
  validate();
  if (the_leaf && !mode(ReadOnly)) {
    the_leaf->commit();
    validate();
    if (mode(Verbose))
      cout << "leaf committed" << endl;
  }
}

/*
  Remove the underlying leaf's file and unload the leaf.  In the
  process, load the leaf if we need to.  If we later try to read from
  the leaf, we'll get an error, but writing will succeed (and will
  recreate the leaf).
*/
void LeafProxy::erase() {
  validate();
  if (!the_leaf)
    // Initialize without loading
    the_leaf = new Leaf(password, input_base_name, input_dir_name, false);
  the_leaf->erase(); // Marks it unmodified, so deleting won't rewrite it.
  delete_leaf();
  validate();
  if (mode(Verbose))
    cout << "leaf erased" << endl;
}

/*
  Load the leaf if its file exists.  Otherwise just initialize the leaf.
*/
void LeafProxy::init_leaf(bool do_load) const {
  validate();
  if (the_leaf)
    return;
  the_leaf = new Leaf(password, input_base_name, input_dir_name, do_load);
  validate();
}

/*
  Clean up the_leaf pointer.
*/
void LeafProxy::delete_leaf() const {
  if (the_leaf)
    delete the_leaf;
  the_leaf = NULL;
}

/*
  Confirm that all is well.

  If force_load is true, we load every leaf to make sure it really
  does load and is consistent.
*/
void LeafProxy::validate(bool force_load) const {
  if (force_load) {
    init_leaf(); // which will in turn re-call validate()
    return;
  }
  if (!the_leaf)
    return;
  assert(valid);
  assert((void *)the_leaf > (void *)0x1FF); // kludge, catch some bad pointers
  // Is it worth making a password accessor in leaf just to check this?
  // Indeed, does it even need to be true?  We could, in priciple, have
  // different passwords for each leaf.
  // assert(password == the_leaf->password);
  if (the_leaf->is_loaded() &&
      ("" != cached_key && cached_key != the_leaf->key()))
    throw(runtime_error("Bad cached key."));
  the_leaf->validate();
}
