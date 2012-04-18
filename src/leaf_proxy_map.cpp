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



#include "srd.h"



using namespace srd;
using namespace std;



/*
  Return leaf proxies for all leaves whose key matches pattern.
  This is far more efficient than looking at payloads, as the leaf
  proxy caches the key, avoiding the need to load the leaf.  So we
  always begin by doing key search.

  Key search is always a disjunction on the pattern space.
*/
LeafProxyMap LeafProxyMap::filter_keys(const srd::vector_string &patterns,
                                       const bool exact,
                                       const StringMatcher &in_matcher)
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
                        if((exact && in_matcher(*pat_it, proxy.key()))
                           || (!exact && in_matcher.contains(proxy.key(), *pat_it))) {
                                found_in_this_proxy = true;
                                results[it->first] = proxy;
                        }
                }
        }
        return results;
}



/*
  Return leaf proxies for all leaves whose payload matches pattern.

  Payload search is a conjunction on the pattern space unless disjunction == true.
*/
LeafProxyMap LeafProxyMap::filter_payloads(const vector_string &patterns,
                                           const bool disjunction,
                                           const StringMatcher &in_matcher)
{
        if(0 == patterns.size())
                // Empty pattern set should pass everything rather than exclude everything.
                return *this;
        LeafProxyMap results = LeafProxyMap();
        for(iterator it = begin(); it != end(); it++) {
                LeafProxy &proxy = (*it).second;
                if(disjunction) {
                        // disjunction
                        bool found_in_this_proxy = false;
                        for(vector_string::const_iterator pat_it = patterns.begin();
                            !found_in_this_proxy && pat_it != patterns.end();
                            pat_it++) {
                                if(in_matcher.contains(proxy.payload(), *pat_it)) {
                                        found_in_this_proxy = true;
                                        results[it->first] = proxy;
                                }
                        }
                } else {
                        // conjunction
                        bool found_in_this_proxy = true;
                        for(vector_string::const_iterator pat_it = patterns.begin();
                            found_in_this_proxy && pat_it != patterns.end();
                            pat_it++) {
                                if(!in_matcher.contains(proxy.payload(), *pat_it))
                                        found_in_this_proxy = false;
                        }                        
                        if(found_in_this_proxy)
                                results[it->first] = proxy;
                }
        }
        return results;
}



/*
  Return leaf proxies for all leaves whose key matches key_pattern or
  whose payload matches payload_pattern.
*/
LeafProxyMap LeafProxyMap::filter_keys_or_payloads(const vector_string &patterns,
                                                   const bool exact,
                                                   const StringMatcher &in_matcher)
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
                        if((exact && in_matcher(proxy.key(), *pat_it))
                           || (!exact && in_matcher.contains(proxy.key(), *pat_it))) {
                                found_in_this_proxy = true;
                                results[it->first] = proxy;
                        }
                }
                for(vector_string::const_iterator pat_it = patterns.begin();
                    !found_in_this_proxy && pat_it != patterns.end();
                    pat_it++) {
                        if(in_matcher.contains(proxy.payload(), *pat_it)) {
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

