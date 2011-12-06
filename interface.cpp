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



/*
  Here find a public interface for libsrd.so.
*/


#include <map>
#include <string>
#include <vector>

#include "interface.h"
#include "root.h"
#include "types.h"


using namespace srd;
using namespace std;



/*
  Fetch the root and filter as requested.
*/
static leaf_proxy_map do_filter(const string password,
                                const vector_string match_key,
                                const vector_string match_payload,
                                const vector_string match_or,
                                const bool match_exact)
{
        root root(password, "");
        leaf_proxy_map lpm = root.filter_keys_and_payloads(match_key, match_payload);
        if(match_or.size())
                lpm = lpm.filter_keys_or_payloads(match_or, match_or);
        return lpm;
}


/*
  Fetch the root and filter as requested.  Return a vector of matching keys.
*/
vector_string filter_to_keys(const string password,
                             const vector_string match_key,
                             const vector_string match_payload,
                             const vector_string match_or,
                             const bool match_exact)
{
        leaf_proxy_map lpm = do_filter(password,
                                       match_key,
                                       match_payload,
                                       match_or,
                                       match_exact);
        vector_string vs;
        //for_each(lpm.begin(), lpm.end(), back_inserter(vs));
        for(leaf_proxy_map::const_iterator it = lpm.begin();
            it != lpm.end();
            ++it) {
                leaf_proxy lp = it->second;
                vs.push_back(lp.key());
        }
        return vs;
}


vector_string filter_to_keys_sub(root root,
                                 const vector_string match_key,
                                 const vector_string match_payload,
                                 const vector_string match_or,
                                 const bool match_exact)
{
        return vector_string();
}



/*
  Fetch the root and filter as requested.  Return a map of matching
  key/payload pairs.
*/
map<string, string> filter_to_records(const string password,
                                      const vector_string match_key,
                                      const vector_string match_payload,
                                      const vector_string match_or,
                                      const bool match_exact)
{
        leaf_proxy_map lpm = do_filter(password,
                                       match_key,
                                       match_payload,
                                       match_or,
                                       match_exact);
        map<string, string> m;
        //for_each(lpm.begin(), lpm.end(), inserter(m, m.begin()));
        for(leaf_proxy_map::const_iterator it = lpm.begin();
            it != lpm.end();
            ++it) {
                leaf_proxy lp = it->second;
                m[lp.key()] = lp.payload();
        }
        return m;
}

