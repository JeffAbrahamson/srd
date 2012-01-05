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



#include "leaf_proxy.h"
#include "leaf_proxy_map.h"



using namespace srd;
using namespace std;



/*
  Return leaf proxies for all leaves whose key matches pattern.
  This is far more efficient than looking at payloads, as the leaf
  proxy caches the key, avoiding the need to load the leaf.  So we
  always begin by doing key search.
*/
LeafProxyMap LeafProxyMap::filter_keys(srd::vector_string patterns, bool exact)
{
        if(0 == patterns.size())
                // Empty pattern set should pass everything rather than exclude everything.
                return *this;
        LeafProxyMap results = LeafProxyMap();
        for(iterator it = begin(); it != end(); it++) {
                LeafProxy &proxy = (*it).second;
                bool found_in_this_proxy = false;
                for(vector_string::const_iterator pat_it = patterns.begin();
                    !found_in_this_proxy && pat_it != patterns.end();
                    pat_it++) {
                        if((exact && *pat_it == proxy.key())
                           || (!exact && proxy.key().find(*pat_it) != string::npos)) {
                                found_in_this_proxy = true;
                                results[it->first] = proxy;
                        }
                }
        }
        return results;
}



/*
  Return leaf proxies for all leaves whose payload matches pattern.
*/
LeafProxyMap LeafProxyMap::filter_payloads(vector_string patterns)
{
        if(0 == patterns.size())
                // Empty pattern set should pass everything rather than exclude everything.
                return *this;
        LeafProxyMap results = LeafProxyMap();
        for(iterator it = begin(); it != end(); it++) {
                LeafProxy &proxy = (*it).second;
                bool found_in_this_proxy = false;
                for(vector_string::const_iterator pat_it = patterns.begin();
                    !found_in_this_proxy && pat_it != patterns.end();
                    pat_it++) {
                        if(proxy.payload().find(*pat_it) != string::npos) {
                                found_in_this_proxy = true;
                                results[it->first] = proxy;
                        }
                }
        }
        return results;
}



/*
  Return leaf proxies for all leaves whose key matches key_pattern or
  whose payload matches payload_pattern.
*/
LeafProxyMap LeafProxyMap::filter_keys_or_payloads(vector_string key_patterns,
                                                       vector_string payload_patterns,
                                                       bool exact)
{
        if(0 == key_patterns.size() && 0 == payload_patterns.size())
                // Empty pattern set should pass everything rather than exclude everything.
                return *this;
        if(0 == key_patterns.size())
                return filter_keys(key_patterns, exact);
        if(0 == payload_patterns.size())
                return filter_payloads(payload_patterns);
        
        LeafProxyMap results = LeafProxyMap();
        for(iterator it = begin(); it != end(); it++) {
                LeafProxy &proxy = (*it).second;
                bool found_in_this_proxy = false;
                for(vector_string::const_iterator pat_it = key_patterns.begin();
                    !found_in_this_proxy && pat_it != key_patterns.end();
                    pat_it++) {
                        if((exact && *pat_it == proxy.key())
                           || (!exact && proxy.key().find(*pat_it) != string::npos)) {
                                found_in_this_proxy = true;
                                results[it->first] = proxy;
                        }
                }
                for(vector_string::const_iterator pat_it = payload_patterns.begin();
                    !found_in_this_proxy && pat_it != payload_patterns.end();
                    pat_it++) {
                        if(proxy.payload().find(*pat_it) != string::npos) {
                                found_in_this_proxy = true;
                                results[it->first] = proxy;
                        }
                }
                
        }
        return results;
}



LeafProxyMap::LPM_Set LeafProxyMap::as_set() const
{
        set<LeafProxy, less<LeafProxy> > s;
        for(LeafProxyMap::const_iterator it = begin();
            it != end();
            it++) {
                LeafProxy lp = it->second;
                s.insert(it->second);
        }
        return s;
}

