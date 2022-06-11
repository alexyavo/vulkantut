#pragma once

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp


#include <stdint.h> // numeric_limits<uint32_t>::max() doesn't work, use UINT_MAX instead

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <fstream>
#include <format>
#include <functional>
#include <algorithm>
#include <sstream>

namespace utils {
  using namespace std;

  template<typename T, typename F, template <typename...> typename C, typename... Args>
  decltype(auto) map(const C<T, Args...>& coll, F f) {
    C<decltype(f(*coll.begin()))> res(coll.size());
    transform(coll.cbegin(), coll.cend(), res.begin(), f);
    return res;
  }

  template<typename T>
  set<T> to_set(const vector<T>& vec) {
    return set(vec.cbegin(), vec.cend());
  }

  template<typename T>
  bool is_subset_of(const set<T>& subset, const set<T>& of) {
    return std::includes(of.cbegin(), of.cend(), subset.cbegin(), subset.cend());
  }

  template<typename T, template <typename...> typename C, typename... Args>
  string to_str(
    const C<T, Args...>& coll,
    const char* prefix = "",
    const char* sep = ",",
    const char* suffix = ""
  ) {
    ostringstream stream;
    stream << prefix;
    bool is_first = true;
    for (auto it = coll.begin(); it != coll.end(); ++it) {
      if (!is_first) {
        stream << sep;
      }

      stream << *it;
      is_first = false;
    }
    stream << suffix;
    return stream.str();
  }

  template<typename T>
  string to_str(const vector<T>& vec) {
    return to_str(vec, "[", ",", "]");
  }

  template<typename T>
  string to_str(const set<T>& s) {
    return to_str(s, "{", ",", "}");
  }
}
