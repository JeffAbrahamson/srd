/*
  Copyright 2012  Jeff Abrahamson

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

#ifndef __SRD_H__
#define __SRD_H__ 1

#include <algorithm>
#include <assert.h>
#include <boost/algorithm/string/case_conv.hpp>
#include <compare>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "leaf.pb.h"
#include "root.pb.h"

namespace srd {

/* ************************************************************ */
/* Compression */

std::string compress(const std::string &);
std::string decompress(const std::string &);

/* ************************************************************ */
/* Encryption */

// Compute a hash (message digest).
std::string message_digest(std::string_view message,
                           const bool filesystem_safe = false);

// Return a (not necessarily human readable) string of random bits.
std::string pseudo_random_string(int length);

// Crypto++ is documented at http://www.cryptopp.com/
// and http://www.cryptopp.com/fom-serve/cache/1.html
// and http://www.cryptopp.com/wiki/FAQ

/*
  Format 1 encryption:  AES-128-CBC keyed by (a prefix of) password,
  with an IV derived from it.  Kept only to read and convert old
  databases.
*/
std::string format1_encrypt(const std::string &plain_message,
                            std::string_view password);
std::string format1_decrypt(const std::string &cipher_message,
                            std::string_view password);
// The format 1 password for a passphrase.
std::string format1_password(const std::string &passphrase);

/*
  Format 2 primitives, built on libsodium.

  A database's passphrase and salt give a master key (Argon2id).  The
  master key gives subkeys (BLAKE2b, via crypto_kdf), one per purpose,
  so that no key serves two purposes.  Files are encrypted with
  XChaCha20-Poly1305 under a fresh random nonce on every write, which
  authenticates them as well:  a changed or truncated file fails to
  decrypt.
*/

// Argon2id cost parameters and salt for deriving a master key.
struct KdfParams {
  std::uint64_t opslimit;
  std::uint64_t memlimit; // bytes
  std::string salt;       // crypto_pwhash_SALTBYTES random bytes
};

// Parameters, with a fresh random salt, for a new database.
KdfParams new_kdf_params();
// Throw runtime_error if params are too weak or too costly to use.
void check_kdf_params(const KdfParams &params);
// Derive the 32-byte master key.  Slow and memory-hungry, by design.
std::string derive_master_key(const std::string &passphrase,
                              const KdfParams &params);

// Subkey identifiers.  Never renumber them:  each identifies a key.
enum SubkeyId : std::uint64_t {
  EncryptionSubkey = 1,
  RootNameSubkey = 2,
};
std::string derive_subkey(const std::string &master_key, SubkeyId id);

// A keyed hash (BLAKE2b) of message, as 64 hex digits:  for instance,
// a file name that only the holder of key can compute.
std::string keyed_hash(std::string_view key, const std::string &message);

/*
  Authenticated encryption under a 32-byte key.  The context (for
  instance, a role and the file's name) is authenticated but not
  stored:  decryption fails unless given the same context, so a file
  copied to another name won't decrypt.  aead_decrypt() throws
  runtime_error on any failure.
*/
std::string aead_encrypt(const std::string &plain_text, std::string_view key,
                         const std::string &context);
std::string aead_decrypt(const std::string &cipher_text, std::string_view key,
                         const std::string &context);

/*
  The key to one database's files, shared by its root and leaves.

  A format 2 key is the encryption subkey, and the file's context
  (role and name) is authenticated.  A format 1 key is the legacy
  password string; it ignores the context.

  The secret lives in memory from sodium_malloc():  locked so it
  can't be swapped out, guarded against overruns, read-only once set,
  and wiped when the key is destroyed.  Copies made elsewhere (the
  passphrase as typed, intermediate hashes) are wiped by their
  owners, but std::string offers no guarantees, so this is
  best-effort; core dumps are disabled as well.
*/
class Key {
public:
  enum Format { Format1 = 1, Format2 = 2 };

  Key(Format format, std::string_view secret);
  ~Key();
  Key(const Key &) = delete;
  Key &operator=(const Key &) = delete;

  Format format() const { return m_format; }
  std::string encrypt(const std::string &plain_text,
                      const std::string &context) const;
  std::string decrypt(const std::string &cipher_text,
                      const std::string &context) const;
  // keyed_hash() under this key.
  std::string keyed_hash(const std::string &message) const;

private:
  std::string_view secret() const;

  const Format m_format;
  unsigned char *m_secret;
  const size_t m_size;
};
typedef std::shared_ptr<const Key> KeyPtr;

/* ************************************************************ */
/* mode */

/*
  Maintain a map of modes.

  Written for verbose and testing, to avoid having to pass them around
  everywhere in case we need them deep down.  Especially verbose.

  Some might call this a kludge or even inelegant.  Abused, it would be.
*/

enum Mode {
  Verbose,
  Testing,
  ReadOnly,
};

void mode(const Mode m, const bool new_state);
bool mode(const Mode m);

/* ************************************************************ */
/* types */

typedef std::vector<std::string> vector_string;
typedef std::pair<time_t, unsigned long int>
    time_pair; /* (seconds, nanoseconds) */

/* ************************************************************ */
/* File */

void set_base_dir(const std::string &in_dir);
// Throw unless dir is ours and only we may change it.
void check_database_dir(const std::string &dir);

/*
  A simple mix-in class to handle reading and writing (binary) files,
  as well as testing for existence and removing them.

  This could surely be done more cleverly.  Or already has been.  (Cf. boost
  filesystem)

  At construction time, we can optionally specify a directory and a name
  (base_name) for the file.  Otherwise, these are determined following
  policy for the srd application.
*/
class File {

public:
  File(const std::string base_name = std::string(),
       const std::string dir_name = std::string());
  ~File(){};

  std::string dirname();
  void dirname(const std::string &in) { m_dir_name = in; }

  std::string basename();
  void basename(const std::string &in) { m_base_name = in; }

  std::string full_path() { return dirname() + "/" + basename(); }

  void file_contents(const std::string &data);
  std::string file_contents();

  time_pair modtime(const bool silent = true);
  bool underlying_is_modified();

  void rm();
  bool exists(); /* Can't be const, because asking for
                    the name might generate a name. */
  bool is_writeable();
  bool dir_is_writeable();

private:
  struct FileStamp {
    dev_t device;
    ino_t inode;
    off_t size;
    time_pair modified;

    bool operator==(const FileStamp &) const = default;
  };

  FileStamp file_stamp(const bool silent = true);

  std::string m_dir_name;
  std::string m_base_name;

  // Once true, we no longer check to see if the directory exists.
  // If false, we'll check and create if needed.
  bool m_dir_verified;

  // Remember the file we read or wrote.  Writes replace files by
  // rename, so identity as well as modification time matters.
  bool m_stamp_known;
  FileStamp m_stamp;
};

/* ************************************************************ */
/* Header */

/*
  A database directory's header file, srd.header, holds what's needed
  to derive keys from a passphrase:  the format version, the Argon2id
  costs, and a random salt.  It's written once, when the first format
  2 database in the directory is created, and never changes, so
  databases sharing the directory share the salt.

  The header isn't authenticated, and needn't be:  changing it changes
  every key derived from it, so the roots can no longer be found.
  check_kdf_params() refuses costs too weak to trust.
*/
class Header : public File {
public:
  static const std::string file_name;

  explicit Header(const std::string &dir_name)
      : File(file_name, dir_name) {}

  // Read and check the header.  It must exist.
  KdfParams params();
  // Read the header, or create it if absent.  Holds the database lock.
  KdfParams params_or_create();

  static std::string encode(const KdfParams &params);
  static KdfParams decode(const std::string &data);
};

/* ************************************************************ */
/* Leaf */

/*
  Represent a data node, one of the objects the user thinks of
  as what we do.  On destruction, or when explicitly requested
  to do so, persist our data (via the inherited file object).

  We have a key and a payload.  For purposes of being useful,
  we keep the database's key, shared with the root and the
  other leaves.
*/
class Leaf : public File {
public:
  Leaf(const KeyPtr &key, const std::string base_name = std::string(),
       const std::string dir_name = std::string(), const bool do_load = true);
  virtual ~Leaf();

  void commit();
  void erase();

  void key(const std::string &key_in) {
    m_node_key = key_in;
    m_modified = true;
    m_loaded = true;
  };
  const std::string key() const {
    assert(m_loaded);
    return m_node_key;
  };
  void payload(const std::string &payload_in) {
    m_node_payload = payload_in;
    m_modified = true;
    m_loaded = true;
  };
  const std::string payload() const {
    assert(m_loaded);
    return m_node_payload;
  };

  void validate();
  bool is_loaded() const { return m_loaded; };

private:
  void load();

  const KeyPtr m_key;
  bool m_modified;
  // m_loaded is true if the leaf has been loaded from its underlying file
  // or if the leaf is new (and perhaps hasn't been persisted yet).
  bool m_loaded;

  std::string m_node_key;
  std::string m_node_payload;
};

/* ************************************************************ */
/* LeafProxy */

/*
  A leaf, meaning a data node, but not necessarily loaded.
  The actual data handling and persistence is done by the leaf
  class.  We make a pointer to a leaf when we load one, and we
  delete the leaf object and null the pointer when we don't
  have it loaded.
*/
class LeafProxy {

public:
  LeafProxy(); // needed by std::map::operator[]()
  LeafProxy(const KeyPtr &key,
            const std::string base_name = std::string(),
            const std::string dir_name = std::string());
  /*
    Deleting the leaf pointer will cause the leaf to
    persist if appropriate.  It would be perverse to
    have multiple leaf_proxy's for the same leaf and
    also to modify more than one of them.  So we don't
    go to the effort to make sure this doesn't happen.
    It's perfectly reasonable to have multiple
    leaf_proxy's for the same leaf as long as no more
    than one is considered writeable.

    The copy constructor and assignment operators
    support this by not copying the_leaf pointer.
  */
  ~LeafProxy() { delete_leaf(); };

  LeafProxy(const LeafProxy &);
  LeafProxy &operator=(const LeafProxy &);

  /*
    All this const messiness is sad, but hard to fix today.
    It's because calling key() can load the leaf itself, which
    means this isn't const.

    Records order by key.
  */
  std::strong_ordering operator<=>(const LeafProxy &rhs) const {
    return key() <=> rhs.key();
  }

  bool set(const std::string &in_key, const std::string &in_payload);

  void key_cache(const std::string &in) {
    cached_key = in;
    validate();
  }
  void key(const std::string &in_key);
  std::string key() const;
  void payload(const std::string &in_payload);
  std::string payload() const;

  void print_key() const;
  void print_payload(const std::string &pattern) const;

  std::string basename() const;
  void commit();
  void erase();

  void validate(bool force_load = false) const;

private:
  void init_leaf(bool do_load = true) const;
  void delete_leaf() const;

  KeyPtr key_ptr; // should be const but for operator=()

  // input_base_name and input_dir_name exist to create
  // the leaf, but we consult the leaf for actual
  // values.
  std::string input_base_name;
  std::string input_dir_name;
  bool valid;

  // We cache the_leaf.key in cached_key so that we don't need to
  // load all leaves for what is likely the most common type of search.
  // This means that the root must persist the cached key value.
  std::string cached_key;
  mutable Leaf *the_leaf;
};

/* ************************************************************ */
/* LeafProxyMap */

/*
  This is the fundamental data structure of a root node.
  It's also how we pass results sets around so that we
  can continue to filter on them.

  The key is the base_name for a leaf_proxy.
  The associated value is a pointer to a leaf proxy.
  The file_name is generated arbitrarily when a leaf_proxy
  is added, so that changing the key or its payload does not affect the
  root node.  Thus, this map's keys is what we persist to the root file
  (cf. root.h, root.cpp).

  This is a separate class so that we can filter on search results.
*/

// Helper classes for abstracting string match
struct StringMatcher {
  virtual ~StringMatcher() {}
  // Return true if in_a and in_b match.
  virtual bool operator()(const std::string &in_a,
                          const std::string &in_b) const = 0;
  // Return true if in_a contains in_b.
  virtual bool contains(const std::string &in_a,
                        const std::string &in_b) const = 0;
};

struct IdentStringMatcher : public StringMatcher {
  bool operator()(const std::string &in_a,
                  const std::string &in_b) const override {
    return in_a == in_b;
  }
  bool contains(const std::string &in_a,
                const std::string &in_b) const override {
    return in_a.contains(in_b);
  }
};

struct UpperStringMatcher : public StringMatcher {
  bool operator()(const std::string &in_a,
                  const std::string &in_b) const override {
    return boost::algorithm::to_upper_copy(in_a) ==
           boost::algorithm::to_upper_copy(in_b);
  }
  bool contains(const std::string &in_a,
                const std::string &in_b) const override {
    return boost::algorithm::to_upper_copy(in_a).contains(
        boost::algorithm::to_upper_copy(in_b));
  }
};

class LeafProxyMap {
  // public std::iterator<std::random_access_iterator_tag,
  // LeafProxyMapInternalType::value_type>

public:
  typedef std::map<std::string, LeafProxy> LeafProxyMapInternalType;

  typedef LeafProxyMapInternalType::iterator iterator;
  typedef LeafProxyMapInternalType::const_iterator const_iterator;
  typedef LeafProxyMapInternalType::key_type key_type;
  typedef LeafProxyMapInternalType::mapped_type mapped_type;
  typedef LeafProxyMapInternalType::size_type size_type;

  // typedef LeafProxyMapInternalType::iterator_category iterator_category;
  typedef LeafProxyMapInternalType::value_type value_type;
  typedef LeafProxyMapInternalType::difference_type difference_type;
  typedef LeafProxyMapInternalType::pointer pointer;
  typedef LeafProxyMapInternalType::reference reference;

  LeafProxyMap(){};
  virtual ~LeafProxyMap(){};

  // Need copy and assignment operators due to the_map.
  LeafProxyMap(const LeafProxyMap &lpm) { the_map = lpm.the_map; }
  LeafProxyMap &operator=(const LeafProxyMap &lpm) {
    LeafProxyMap temp(lpm);
    the_map.swap(temp.the_map);
    return *this;
  }

  LeafProxyMap filter_keys(const srd::vector_string &, const bool exact,
                           const StringMatcher &in_matcher);
  LeafProxyMap filter_payloads(const srd::vector_string &,
                               const bool disjunction,
                               const StringMatcher &in_matcher);
  LeafProxyMap filter_keys_or_payloads(const srd::vector_string &,
                                       const bool exact,
                                       const StringMatcher &in_matcher);

  // All the leaf proxies, sorted by key.  Records may share a key, so
  // this is a list, not a set.
  typedef std::vector<LeafProxy> LeafProxyList;
  LeafProxyList sorted_by_key() const;

  /* Functions that proxy to the_map. */
  void clear() { the_map.clear(); }
  bool empty() const { return the_map.empty(); };

  void erase(iterator pos) { the_map.erase(pos); };
  size_type erase(const key_type &k) { return the_map.erase(k); };
  void erase(iterator first, iterator last) { the_map.erase(first, last); };

  iterator find(const key_type &k) { return the_map.find(k); }
  const_iterator find(const key_type &k) const { return the_map.find(k); }
  size_type size() const { return the_map.size(); }

  mapped_type &operator[](const key_type &k) { return the_map[k]; }

  iterator begin() { return the_map.begin(); }
  const_iterator begin() const { return the_map.begin(); }
  iterator end() { return the_map.end(); }
  const_iterator end() const { return the_map.end(); }

private:
  LeafProxyMapInternalType the_map;
};

/* ************************************************************ */
/* Root */

/*
  The root node, which persists to a file, tracks the leaf nodes.
  The thing we actually persist is just the set of leaf names.
  At runtime, the thing we have is a map from leaf names to leaf proxy objects.

  The root node supports filter operations on the leaf proxies (and
  so on the leaf objects themselves).

  If create is false, the root's underlying file must already exist.
  If it is true, the root file must not exist and is created.
*/
class Root : public File, public LeafProxyMap {
public:
  Root(const std::string &passphrase, const std::string path = std::string(),
       const bool create = false);
  virtual ~Root();

  // A leaf (or leaf proxy) contains a key and payload.
  // But the root node contains proxy keys, which serve to identify
  // the actual leaf, in which we have stored the real key.
  void add_leaf(const std::string &key, const std::string &payload,
                const bool do_commit = true);
  LeafProxy get_leaf(const std::string &proxy_key);
  void set_leaf(const std::string &proxy_key, const std::string &key,
                const std::string &payload);
  void rm_leaf(const std::string &proxy_key);

  Root change_password(const std::string &new_passphrase);
  void commit();
  void validate(bool force_load = false) const;
  void checksum() const;
  // A keyed hash of text, for checksums.
  std::string checksum_of(const std::string &text) const;

private:
  void load();
  void refresh_for_update();
  void find_key(const std::string &passphrase);
  void create_key(const std::string &passphrase);
  void use_format2(const std::string &passphrase, const KdfParams &params);
  void use_format1(const std::string &passphrase);

  // Data members
  KeyPtr db_key;
  std::string checksum_key;
  bool modified;
  bool valid; // if false, all operations except deletion should fail
  bool persisted; // true once this root has been read from or written to disk
};

/* ************************************************************ */
/* Lock */

/*
  An exclusive lock on a database directory.  Hold it from reloading
  the root through writing it back, so that concurrent srd processes
  can't lose each other's changes.

  Loading the root takes the lock too, briefly, so that no record
  disappears between reading the root and validating it.  Record
  contents are read later without it; every file is replaced by an
  atomic rename, so those reads always see whole files.

  Re-entrant within a process:  the lock is released when the
  outermost Lock is destroyed.  In read-only mode, an existing lock
  file is respected but none is created.
*/
class Lock {

public:
  explicit Lock(const std::string &dir_name);
  ~Lock();
  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;

private:
  std::string m_filename; /* Name of the lock file */
  bool m_held;
  std::pair<dev_t, ino_t> m_id; /* Identity of the lock file */
};

/* ************************************************************ */
/* FileUtil */

void file_create(const std::string &filename);
void file_create_private(const std::string &filename, const std::string &data);
void file_write_all(int fd, const std::string &data,
                    const std::string &filename);
bool file_exists(const std::string &filename);
bool is_safe_file_name(const std::string &name);
void file_rm(const std::string &filename);
}

#endif /* __SRD_H__*/
