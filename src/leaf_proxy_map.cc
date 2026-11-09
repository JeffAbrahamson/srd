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

#include <algorithm>
#include <iostream>
#include <ranges>

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
                                       const StringMatcher &in_matcher) {
  if (patterns.empty())
    // Empty pattern set should pass everything rather than exclude everything.
    return *this;
  LeafProxyMap results;
  for (const auto &[name, proxy] : *this) {
    const string key = proxy.key();
    if (ranges::any_of(patterns, [&](const string &pattern) {
          return exact ? in_matcher(pattern, key)
                       : in_matcher.contains(key, pattern);
        }))
      results[name] = proxy;
  }
  return results;
}

/*
  Return leaf proxies for all leaves whose payload matches pattern.

  Payload search is a conjunction on the pattern space unless disjunction ==
  true.
*/
LeafProxyMap LeafProxyMap::filter_payloads(const vector_string &patterns,
                                           const bool disjunction,
                                           const StringMatcher &in_matcher) {
  if (patterns.empty())
    // Empty pattern set should pass everything rather than exclude everything.
    return *this;
  LeafProxyMap results;
  for (const auto &[name, proxy] : *this) {
    const string payload = proxy.payload();
    auto matches = [&](const string &pattern) {
      return in_matcher.contains(payload, pattern);
    };
    if (disjunction ? ranges::any_of(patterns, matches)
                    : ranges::all_of(patterns, matches))
      results[name] = proxy;
  }
  return results;
}

/*
  Return leaf proxies for all leaves whose key matches key_pattern or
  whose payload matches payload_pattern.
*/
LeafProxyMap
LeafProxyMap::filter_keys_or_payloads(const vector_string &patterns,
                                      const bool exact,
                                      const StringMatcher &in_matcher) {
  if (patterns.empty())
    // Empty pattern set should pass everything rather than exclude everything.
    return *this;

  LeafProxyMap results;
  for (const auto &[name, proxy] : *this) {
    const string key = proxy.key();
    // Check every key before loading the payload.
    if (ranges::any_of(patterns,
                       [&](const string &pattern) {
                         return exact ? in_matcher(key, pattern)
                                      : in_matcher.contains(key, pattern);
                       }) ||
        ranges::any_of(patterns, [&](const string &pattern) {
          return in_matcher.contains(proxy.payload(), pattern);
        }))
      results[name] = proxy;
  }
  return results;
}

/*
  Return all leaf proxies sorted by key.  The sort is stable, so
  records with the same key stay in (arbitrary but repeatable) proxy
  name order.
*/
LeafProxyMap::LeafProxyList LeafProxyMap::sorted_by_key() const {
  auto leaves = *this | views::values | ranges::to<LeafProxyList>();
  ranges::stable_sort(leaves, {}, [](const LeafProxy &lp) { return lp.key(); });
  return leaves;
}
