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
#include <functional>
#include <stdlib.h>
#include <string.h>
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

  The name of the root node must be determinable solely by the
  passphrase (and, in format 2, the directory's header).
*/
Root::Root(const string &passphrase, const string dir_name, const bool create)
    : Root(passphrase, dir_name, create, false) {}

/*
  As above.  If replacing_format1, the format 1 database that
  passphrase names may exist:  we're about to replace it.
*/
Root::Root(const string &passphrase, const string &dir_name, const bool create,
           const bool replacing_format1)
    : modified(false), valid(true), persisted(false) {
  dirname(dir_name); // If empty, will be computed for us
  check_database_dir(dirname());
  if (create)
    create_key(passphrase, replacing_format1);
  else
    find_key(passphrase);
  persisted = !create;
  if (!is_writeable())
    mode(ReadOnly, true);
  if (create) {
    checksum_key = pseudo_random_string(32);
    cout << "Root node does not exist, will create." << endl;
    if (mode(Verbose))
      cout << "    [" << full_path() << "]" << endl;
    modified = true;
    validate();
    return;
  }
  load();
  validate();
}

namespace {
const string already_exists_message(
    "A database identified by this password already exists.");

/*
  Format 1:  the root's name is 30 more hashes of the password.
*/
string format1_root_name(const string &password) {
  string base_name(password);
  for (int i = 0; i < 30; i++)
    base_name = message_digest(base_name, true);
  return base_name;
}
}

/*
  Find the database that passphrase names:  format 2 if the directory
  has a header, else (or failing that) format 1.  Set our key and
  name, or throw if there's no such database.
*/
void Root::find_key(const string &passphrase) {
  Header header(dirname());
  string header_error;
  if (header.exists()) {
    try {
      use_format2(passphrase, header.params());
      if (exists())
        return;
    } catch (const runtime_error &e) {
      // A bad header mustn't hide format 1 databases beside it.
      header_error = e.what();
    }
  }
  use_format1(passphrase);
  if (exists()) {
    cerr << "Note:  this database is in srd's old, weaker format.  "
            "srd --upgrade converts it."
         << endl;
    return;
  }
  db_key.reset();
  if (!header_error.empty())
    throw(runtime_error(header_error));
  throw(runtime_error("Root doesn't exist.  (Incorrect password?)"));
}

/*
  Set our key and name for a new format 2 database, creating the
  directory's header if need be.  Throw if passphrase already names a
  database, in either format (except format 1, if replacing_format1).
*/
void Root::create_key(const string &passphrase, const bool replacing_format1) {
  use_format1(passphrase);
  if (exists() && !replacing_format1)
    throw(runtime_error(already_exists_message));
  Header header(dirname());
  const bool had_header = header.exists();
  if (had_header) {
    use_format2(passphrase, header.params());
    if (exists())
      throw(runtime_error(already_exists_message));
  }
  if (!dir_is_writeable())
    mode(ReadOnly, true);
  if (mode(ReadOnly))
    throw(runtime_error("Refusing to create new root node while read-only."));
  if (!had_header) {
    // Another srd may have created the header meanwhile, perhaps
    // along with this database.
    use_format2(passphrase, header.params_or_create());
    if (exists())
      throw(runtime_error(already_exists_message));
  }
}

/*
  Format 2:  derive the master key from passphrase and the header's
  parameters, and from it the encryption key and the root's name.
*/
void Root::use_format2(const string &passphrase, const KdfParams &params) {
  string master_key = derive_master_key(passphrase, params);
  string name_key = derive_subkey(master_key, RootNameSubkey);
  string encryption_key = derive_subkey(master_key, EncryptionSubkey);
  basename(keyed_hash(name_key, "root"));
  db_key = make_shared<const Key>(Key::Format2, encryption_key);
  explicit_bzero(master_key.data(), master_key.size());
  explicit_bzero(name_key.data(), name_key.size());
  explicit_bzero(encryption_key.data(), encryption_key.size());
}

/*
  Format 1:  the password is the key.
*/
void Root::use_format1(const string &passphrase) {
  string password = format1_password(passphrase);
  basename(format1_root_name(password));
  db_key = make_shared<const Key>(Key::Format1, password);
  explicit_bzero(password.data(), password.size());
}

/*
  Load the root's contents.
*/
void Root::load() {
  if (modified) {
    cerr << "Uncommitted change to root and external change to root file.  "
            "Data will be lost."
         << endl;
    throw(runtime_error("Refusing to modify externally modified root."));
  }
  if (mode(Verbose))
    cout << "Loading root:  " << basename() << endl;
  if (!mode(ReadOnly) && (!is_writeable() || !dir_is_writeable())) {
    cout << "Opening database in read-only mode." << endl;
    mode(ReadOnly, true);
  }
  clear(); // Drop existing LeafProxy's, if any
  // Lock, so that no other process removes a record between our
  // reading the root and validating it.  (Re-entrant:  callers that
  // go on to modify the root already hold it.)
  Lock lock(dirname());
  string plain_text = db_key->decrypt(file_contents(), "root:" + basename());
  persisted = true;
  string big_text = decompress(plain_text);
  RootData root_data;
  if (!root_data.ParseFromString(big_text)) {
    cerr << "Failed to deserialize root." << endl;
    throw(runtime_error("Failed to deserialize root"));
  }
  // TODO(jeff@purple.com): It's a kludge to capture this in the lambda closure.
  for_each(root_data.keys().begin(), root_data.keys().end(),
           [root_data, this](RootData_KeyData key) mutable {
             (*this)[key.proxy_name()] =
                 LeafProxy(db_key, key.proxy_name(), "");
             (*this)[key.proxy_name()].key_cache(key.cached_key());
           });
  assert(size() == static_cast<unsigned int>(root_data.keys_size()));
  // Roots written before checksum keys existed get one derived from
  // the database key, which is stored at the next commit.
  checksum_key = root_data.has_checksum_key() ? root_data.checksum_key()
                                              : db_key->keyed_hash("checksum");
  validate();
}

/*
  Once the database lock is held, bring an unmodified root up to date
  before changing it.  A root that used to exist but has disappeared
  was removed deliberately (notably by a password change); it must
  never be recreated from this process's stale in-memory copy.
*/
void Root::refresh_for_update() {
  if (!persisted) {
    if (exists())
      throw(runtime_error("Refusing to overwrite a database another srd "
                          "created."));
    return;
  }
  if (!exists())
    throw(runtime_error("Refusing to recreate a database another srd "
                        "removed."));
  if (!underlying_is_modified())
    return;
  if (modified)
    throw(runtime_error("Refusing to overwrite changes another srd made "
                        "to the database."));
  load();
}

/*
  Serialize and persist the node before destruction.

  This is a safety net: callers should commit() explicitly, which
  reports failure by throwing.  A destructor must not throw (it may run
  while another exception unwinds), so here we can only complain.
*/
Root::~Root() {
  if (!valid)
    return;
  try {
    validate();
    commit();
  } catch (const exception &e) {
    cerr << "Error:  failed to save the index, recent changes may be lost:  "
         << e.what() << endl;
  }
}

/*
  Add a leaf with key and payload.
  Insert the proxy key and leaf_proxy into the root.
*/
void Root::add_leaf(const string &key, const string &payload,
                    const bool do_commit) {
  // Reconcile or reject external changes while holding the database lock.
  Lock lock(dirname());
  refresh_for_update();
  validate();
  LeafProxy proxy(db_key, "", dirname());
  proxy.set(key, payload);
  (*this)[proxy.basename()] = proxy;
  modified = true; // Adding a leaf requires persisting the root.
  if (do_commit)
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
LeafProxy Root::get_leaf(const string &proxy_key) {
  validate();
  iterator it = find(proxy_key);
  if (end() == it)
    throw(runtime_error("Key not found."));
  return it->second;
}

/*
  Set the contents of an existing leaf.
  To create a new leaf, use add_leaf().
*/
void Root::set_leaf(const string &proxy_key, const string &key,
                    const string &payload) {
  // Reconcile or reject external changes while holding the database lock.
  Lock lock(dirname());
  refresh_for_update();
  validate();
  iterator it = find(proxy_key);
  if (end() == it)
    throw(runtime_error("Key not found."));
  LeafProxy &proxy = it->second;
  if (proxy.set(key, payload)) {
    // The key changed, and the root caches keys.
    modified = true;
    commit();
  }
  validate();
}

/*
  Remove a leaf_proxy from the root, deleting the underlying leaf
  file.
*/
void Root::rm_leaf(const string &proxy_key) {
  if (mode(ReadOnly))
    throw(runtime_error("Database is read-only.  Delete not permitted."));
  // Reconcile or reject external changes while holding the database lock.
  Lock lock(dirname());
  refresh_for_update();
  validate();
  iterator it = find(proxy_key);
  if (end() == it)
    throw(runtime_error("Key not found."));
  // Commit the root before removing the record's file, so that the
  // root on disk never refers to a missing file.
  LeafProxy doomed(it->second);
  erase(it);
  modified = true;
  commit();
  doomed.erase();
  validate();
}

/*
  Change the password associated with this root and all its leaves.

  First make a new root and, for each leaf in the old root, add it to
  the new root.  Once the new root is complete and persisted, remove
  the old root's file, then its leaves, and mark it invalid so that
  any further operations on it will fail.

  The old root goes before its leaves so that an interruption leaves
  at worst some unreferenced leaf files, never an old root that
  refers to missing leaves.

  Return the new root.  Will throw runtime_error if the new password
  generates an existing root object.
*/
Root Root::change_password(const std::string &new_passphrase) {
  return copy_to(new_passphrase, false);
}

/*
  Rewrite this format 1 database as format 2, as a password change
  to the same passphrase.  New databases are always format 2.
*/
Root Root::upgrade(const std::string &passphrase) {
  if (Key::Format1 != format())
    throw(runtime_error("The database is already in the current format."));
  if (format1_root_name(format1_password(passphrase)) != basename())
    throw(logic_error("Upgrade with a passphrase that didn't open the root."));
  return copy_to(passphrase, true);
}

/*
  Copy this database to a new one that passphrase names, then remove
  this one.  See change_password().
*/
Root Root::copy_to(const std::string &passphrase,
                   const bool replacing_format1) {
  // Reconcile or reject external changes while holding the database lock.
  Lock lock(dirname());
  refresh_for_update();
  validate();
  Root new_root(passphrase, dirname(), true, replacing_format1);
  // Keep checksums comparable across the change.
  new_root.checksum_key = checksum_key;
  for (const_iterator it = begin(); it != end(); it++)
    new_root.add_leaf((*it).second.key(), (*it).second.payload(), false);
  new_root.commit();
  validate();
  valid = false;
  rm();
  if (exists())
    throw(runtime_error("The new database is complete, but the old one "
                        "could not be removed."));
  persisted = false;
  while (!empty()) {
    iterator it = begin();
    (*it).second.erase();
    erase(it);
  }
  new_root.validate();
  return new_root;
}

/*
  If we have been modified, persist to our underlying file.
*/
void Root::commit() {
  validate();
  if (!modified || mode(ReadOnly))
    return;
  Lock lock(dirname());
  // Pending changes were made against the root as we last read it.
  // If another process has rewritten it since, writing ours would
  // silently discard theirs.  (Callers that modify the root hold the
  // lock from reload to commit, so this shouldn't happen.)
  if (persisted) {
    if (!exists())
      throw(runtime_error("Refusing to recreate a database another srd "
                          "removed."));
    if (underlying_is_modified())
      throw(runtime_error("Refusing to overwrite changes another srd made "
                          "to the database."));
  } else if (exists()) {
    throw(runtime_error("Refusing to overwrite a database another srd "
                        "created."));
  }

  RootData root_data;
  root_data.set_checksum_key(checksum_key);
  for_each(begin(), end(),
           [&root_data](LeafProxyMapInternalType::value_type val) mutable {
             RootData_KeyData *key_data = root_data.add_keys();
             key_data->set_proxy_name(val.first);
             key_data->set_cached_key(val.second.key());
           });
  string big_text;
  if (!root_data.SerializeToString(&big_text)) {
    cerr << "Failed to serialize root." << endl;
    throw(runtime_error("Failed to serialize root."));
  }
  string plain_text = compress(big_text);
  string cipher_text = db_key->encrypt(plain_text, "root:" + basename());
  file_contents(cipher_text);
  persisted = true;
  modified = false;

  validate();
  if (mode(Verbose))
    cout << "root committed, size=" << size() << endl;
}

/*
  Confirm that all is well.
  It is an error if all is not, and we will die.

  If force_load is true, we load every leaf to make sure it really
  does load and is consistent.
*/
void Root::validate(bool force_load) const {
  assert(valid);
  assert(db_key);
  // Validate each of the leaf proxies.  Note that this won't cause them to
  // load.
  for (const_iterator it = begin(); it != end(); it++) {
    assert((*it).first == (*it).second.basename());
    (*it).second.validate(force_load);
  }
}

/*
  Checksum all keys and payloads together.
*/
void Root::checksum() const {
  validate(true);
  // Sort on content alone, so that the checksum doesn't depend on the
  // (random) file names of records that share a key.
  vector<pair<string, string>> records;
  for (const_iterator it = begin(); it != end(); ++it)
    records.emplace_back(it->second.key(), it->second.payload());
  sort(records.begin(), records.end());
  // Length-prefix each field, so that no two different databases hash
  // the same text:  ("ab", "c") and ("a", "bc") differ.
  string text;
  for (const auto &[key, payload] : records) {
    text += to_string(key.size()) + ":" + key;
    text += to_string(payload.size()) + ":" + payload;
  }
  cout << checksum_of(text) << endl;
}

string Root::checksum_of(const string &text) const {
  assert(!checksum_key.empty());
  return keyed_hash(checksum_key, text);
}
