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
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
//#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>


namespace srd {


    /* ************************************************************ */
    /* Compression */

    std::string compress(const std::string &);
    std::string decompress(const std::string &, unsigned int = 0);


    /* ************************************************************ */
    /* Encryption */
        
    // Compute a hash (message digest).
    std::string message_digest(const std::string &message,
			       const bool filesystem_safe = false);

    // Return a (not necessarily human readable) string of random bits.
    std::string pseudo_random_string(int length);
        
    // Crypto++ is documented at http://www.cryptopp.com/
    // and http://www.cryptopp.com/fom-serve/cache/1.html
    // and http://www.cryptopp.com/wiki/FAQ

    /*
      This is a simple mixin class that provides encryption and decryption.
    */
    std::string encrypt(const std::string &plain_message,
			const std::string &password);
    std::string decrypt(const std::string &cipher_message,
			const std::string &password);


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
    const bool mode(const Mode m);

        
    /* ************************************************************ */
    /* types */
        
    typedef std::vector<std::string> vector_string;
    typedef std::pair<time_t, unsigned long int> time_pair;  /* (seconds, nanoseconds) */
        

    /* ************************************************************ */
    /* File */

    void set_base_dir(const std::string &in_dir);
        
    /*
      A simple mix-in class to handle reading and writing (binary) files,
      as well as testing for existence and removing them.

      This could surely be done more cleverly.  Or already has been.  (Cf. boost filesystem)

      At construction time, we can optionally specify a directory and a name
      (base_name) for the file.  Otherwise, these are determined following
      policy for the srd application.  
    */
    class File {
                
    public:
	File(const std::string base_name = std::string(),
	     const std::string dir_name = std::string());
	~File() {};

	std::string dirname();
	void dirname(const std::string &in) { m_dir_name = in; }
                
	std::string basename();
	void basename(const std::string &in) { m_base_name = in; }
                
	std::string full_path() { return dirname() + "/" + basename(); }
                
	void file_contents(std::string &data, bool lock = true);
	std::string file_contents();

	time_pair modtime(const bool silent = true);
	bool underlying_is_modified();
                
	void rm();
	bool exists();  /* Can't be const, because asking for
			   the name might generate a name. */
	bool is_writeable();
	bool dir_is_writeable();
                
    protected:

	time_pair m_time(const bool silent = true);
                

    private:

	void file_contents_sub(std::string &data);
                
	std::string m_dir_name;
	std::string m_base_name;
                
	// Once true, we no longer check to see if the directory exists.
	// If false, we'll check and create if needed.
	bool m_dir_verified;

	// When first we read the file, check it's mod time.
	// If that changes. we'll know to reread.
	time_pair m_modtime;
    };



    /* ************************************************************ */
    /* Leaf */

    /*
      Represent a data node, one of the objects the user thinks of
      as what we do.  On destruction, or when explicitly requested
      to do so, persist our data (via the inherited file object).

      We have a key and a payload.  For purposes of being useful,
      we keep track of the password.  That's admittedly not a good
      idea, since a core dump would potentially contain the
      password multiple times, which would make it easier to find.
      On the other hand, it's not the human-readable pass phrase
      at least.
    */
    class Leaf : public File {
    public:
	Leaf() { assert(0); };  // seemingly needed by serialize()
	Leaf(const std::string &password,
	     const std::string base_name = std::string(),
	     const std::string dir_name = std::string(),
	     const bool do_load = true);
	virtual ~Leaf();

	void commit();
	void erase();
                
	void key(const std::string &key_in) {
	    m_node_key = key_in;
	    m_modified = true;
	    m_loaded = true;
	};
	const std::string key() const { assert(m_loaded); return m_node_key; };
	void payload(const std::string &payload_in)
	    {
		m_node_payload = payload_in;
		m_modified = true;
		m_loaded = true;
	    };
	const std::string payload() const { assert(m_loaded); return m_node_payload; };

	void validate();
	bool is_loaded() const { return m_loaded; };
                                
    private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive &ar, const unsigned int version);
	void load();
                
	const std::string m_password;
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
	LeafProxy();   // needed by std::map::operator[]()
	LeafProxy(const std::string &password,
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

	  We probably only need operator<() for the compare in
	  LeafProxyMap.  Provide them all to avoid some subtle
	  bug some day.
	*/
	bool operator<=(const LeafProxy& rhs) const
	    {
		return key() <= rhs.key();
	    }

	bool operator<(const LeafProxy& rhs) const
	    {
		return key() < rhs.key();
	    }
        
	bool operator>=(const LeafProxy& rhs) const
	    { 
		return key() >= rhs.key();
	    }

	bool operator>(const LeafProxy& rhs) const
	    {
		return key() > rhs.key();
	    }

	bool set(const std::string &in_key, const std::string &in_payload);

	void key_cache(const std::string &in) { cached_key = in; validate(); }
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
                
	std::string password; // should be const but for operator=()

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


    /*
      A helper object for finding leaves (via LeafProxy's) that
      match given criteria.

      We are relatively simple, supporting only conjunction
      against key and/or conjunction against payload.
    */
    class LeafMatcher {

    public:
	LeafMatcher(const srd::vector_string &,
		    const srd::vector_string &,
		    bool conj);
                
	bool operator()(LeafProxy &);

    private:
	srd::vector_string key;
	srd::vector_string payload;
	bool conjunction;
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
	//virtual const bool operator()(const std::string &in_a, const std::string &in_b) const = 0;
	virtual const bool operator()(const std::string &in_a, const std::string &in_b) const {
	    std::cerr << "():  Fail base" << std::endl; return false; };
	//virtual const bool contains(const std::string &in_a, const std::string &in_b) const = 0;
	virtual const bool contains(const std::string &in_a, const std::string &in_b) const {
	    std::cerr << "contains:  Fail base" << std::endl; return false; };
	//virtual const int id() const { return 1; };
    };
        
    struct IdentStringMatcher : public StringMatcher {
	virtual const bool operator()(const std::string &in_a, const std::string &in_b) const
	    { return in_a == in_b; }
	virtual const bool contains(const std::string &in_a, const std::string &in_b) const
	    { return in_a.find(in_b) != std::string::npos; }
	//virtual const int id() const { return 2; };
    };
        
    struct UpperStringMatcher : public StringMatcher {
	virtual const bool operator()(const std::string &in_a, const std::string &in_b) const
	    { return boost::algorithm::to_upper_copy(in_a) == boost::algorithm::to_upper_copy(in_b); }
	virtual const bool contains(const std::string &in_a, const std::string &in_b) const
	    { return boost::algorithm::to_upper_copy(in_a).find(boost::algorithm::to_upper_copy(in_b)) != std::string::npos; }
	//virtual const int id() const { return 3; };
    };

        
    class LeafProxyMap {
	//public std::iterator<std::random_access_iterator_tag, LeafProxyMapInternalType::value_type>

    public:
	typedef std::map<std::string, LeafProxy> LeafProxyMapInternalType;
                
	typedef LeafProxyMapInternalType::iterator iterator;
	typedef LeafProxyMapInternalType::const_iterator const_iterator;
	typedef LeafProxyMapInternalType::key_type key_type;
	typedef LeafProxyMapInternalType::mapped_type mapped_type;
	typedef LeafProxyMapInternalType::size_type size_type;

	//typedef LeafProxyMapInternalType::iterator_category iterator_category;
	typedef LeafProxyMapInternalType::value_type value_type;
	typedef LeafProxyMapInternalType::difference_type difference_type;
	typedef LeafProxyMapInternalType::pointer pointer;
	typedef LeafProxyMapInternalType::reference reference;
                
	LeafProxyMap() {};
	virtual ~LeafProxyMap() {};

	// Need copy and assignment operators due to the_map.
	LeafProxyMap(const LeafProxyMap &lpm)
	    {
		the_map = lpm.the_map;
	    }
	LeafProxyMap &operator=(const LeafProxyMap &lpm)
	    {
		LeafProxyMap temp(lpm);
		the_map.swap(temp.the_map);
		return *this;
	    }

	LeafProxyMap filter_keys(const srd::vector_string &,
				 const bool exact,
				 const StringMatcher &in_matcher);
	LeafProxyMap filter_payloads(const srd::vector_string &,
				     const bool disjunction,
				     const StringMatcher &in_matcher);
	LeafProxyMap filter_keys_or_payloads(const srd::vector_string &,
					     const bool exact,
					     const StringMatcher &in_matcher);

	typedef std::set<LeafProxy, std::less<LeafProxy> > LPM_Set;
	LPM_Set as_set() const;

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
	Root(const std::string &password,
	     const std::string path = std::string(),
	     const bool create = false);
	virtual ~Root();

	// A leaf (or leaf proxy) contains a key and payload.
	// But the root node contains proxy keys, which serve to identify
	// the actual leaf, in which we have stored the real key.
	void add_leaf(const std::string &key, const std::string &payload,
		      const bool do_commit = true);
	LeafProxy get_leaf(const std::string &proxy_key);
	void set_leaf(const std::string &proxy_key,
		      const std::string &key,
		      const std::string &payload);
	void rm_leaf(const std::string &proxy_key);

	Root change_password(const std::string &new_password);
	void commit();
	void validate(bool force_load = false) const;
	void checksum(bool force_load = false) const;

    private:

	void load();
                
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive &ar, const unsigned int version);

	// Used for serialization only.  A kludge.
	class LeafProxyPersist {
	public:
	    std::string proxy_name;
	    std::string cached_key;
	private:
	    friend class boost::serialization::access;
	    template<class Archive>
	    void serialize(Archive &ar, const unsigned int version)
		{
		    ar & proxy_name & cached_key;
		}
	};
	std::vector<LeafProxyPersist> leaf_names;

	void instantiate_leaf_proxy(LeafProxyPersist &proxy_info);
	void populate_leaf_names(LeafProxyMap::value_type &val);
                
	// Data members

	const std::string password;
	bool modified;
	bool valid;     // if false, all operations except deletion should fail
    };


    /* ************************************************************ */
    /* Lock */

    class Lock {

    public:
                
	Lock(const std::string &filename);
	~Lock();

                
    private:
                
	std::string m_filename; /* Name of the lock file */
	bool m_must_remove;
	boost::interprocess::file_lock *m_lock;
    };



    /* ************************************************************ */
    /* FileUtil */

    void file_create(const std::string &filename);
    bool file_exists(const std::string &filename);
    void file_rm(const std::string &filename);
        
}


#endif  /* __SRD_H__*/
