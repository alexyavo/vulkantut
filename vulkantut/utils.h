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
#include <memory>

namespace utils {
  using namespace std;

  using u32 = uint32_t;

  template<typename T> using ptr = shared_ptr<T>;

  /*
   * what happens if:
   * A has B
   * B has C
   * but A also has internal C of B (for convenience)
   * 
   * A is destroyed
   * releases ref to B first, last ref held to that B 
   * B is being destroyed
   * C remains intact until A completes destruction
   * if C points back / assumes B exists...
   */

  template<typename T, typename... Args>
  inline decltype(auto) mk_ptr(Args&&... args) {
    return make_shared<T>(forward<Args>(args)...);
  }

  template<typename T> using uptr = unique_ptr<T>;

  template<typename T, typename... Args>
  inline decltype(auto) mk_uptr(Args&&... args) {
    return make_unique<T>(forward<Args>(args)...);
  }

  // T isn't ideal? something about perfect parameter forwarding ? 
  template<typename T, typename F, template <typename...> typename C, typename... Args>
  optional<T> find(const C<T, Args...>& coll, F pred) {
    for (auto it = coll.cbegin(); it != coll.cend(); ++it) {
      if (pred(*it)) {
        return *it;
      }
    }
    return {};
  }

  template<typename T, typename F, template <typename...> typename C, typename... Args>
  decltype(auto) map(const C<T, Args...>& coll, F f) {
    // pretty sure this requires the result type to have an empty ctor (in my case, PhysDevice was causing the issue)
    // alternative would be to declare res without size and use push_back?
    // 
    //C<decltype(f(*coll.begin()))> res(coll.size());
    //transform(coll.cbegin(), coll.cend(), res.begin(), f);

    C<decltype(f(*coll.begin()))> res{};
    for (auto it = coll.cbegin(); it != coll.cend(); ++it) {
      res.push_back(f(*it));
    }

    return res;
  }

  // map with index, f(index, element_at_index)
  template<typename T, typename F, template <typename...> typename C, typename... Args>
  decltype(auto) map_enumerated(const C<T, Args...>& coll, F f) {
    C<decltype(f(0, *coll.begin()))> res{};
    for (auto it = coll.cbegin(); it != coll.cend(); ++it) {
      auto idx = static_cast<u32>(it - coll.cbegin());
      res.push_back(f(idx, *it));
    }
    return res;
  }

  template<typename T, typename F, template <typename...> typename C, typename... Args>
  decltype(auto) filter(const C<T, Args...>& coll, F f) {
    C<T> res;
    copy_if(coll.cbegin(), coll.end(), back_inserter(res), f);
    return res;
  }

  template<typename T>
  bool is_subset_of(const set<T>& subset, const set<T>& of) {
    return std::includes(of.cbegin(), of.cend(), subset.cbegin(), subset.cend());
  }

  template<typename T>
  set<T> to_set(const vector<T>& vec) {
    return set(vec.cbegin(), vec.cend());
  }

  template<typename T>
  vector<T> to_vector(const set<T>& s) {
    vector<T> res;
    std::copy(s.cbegin(), s.cend(), std::back_inserter(res));
    return res;
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

  vector<char> read_file(const string& fname) {
    ifstream file(fname, ios::ate | ios::binary);

    if (!file.is_open()) {
      throw runtime_error(format("failed to open {}", fname));
    }

    size_t fileSize = file.tellg();
    vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
  }
}
