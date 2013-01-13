// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: common.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_COMMON_H_
#define CABOCHA_COMMON_H_

#include <setjmp.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>
#include <cmath>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// tricky macro for MSVC
#ifdef _MSC_VER
#define NOMINMAX
#define snprintf _snprintf
#include <windows.h>
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#define COPYRIGHT "CaboCha (Yet Another Japanese Dependency Structure Analyzer)\n\
Copyright(C) 2001-2013 Taku Kudo, All rights reserved.\n"

#define BUF_SIZE 8192
#define CABOCHA_MAX_LINE_SIZE 8192
#define CABOCHA_FEATURE_SIZE 8192 * 16
#define CABOCHA_CHUNK_SIZE 128
#define CABOCHA_TOKEN_SIZE 512

#ifndef CABOCHA_DEFAULT_POSSET
#define CABOCHA_DEFAULT_POSSET  "IPA"
#endif

#ifndef CABOCHA_DEFAULT_CHARSET
#if defined(_WIN32) && ! defined(__CYGWIN__)
#define CABOCHA_DEFAULT_CHARSET "SHIFT-JIS"
#else
#define CABOCHA_DEFAULT_CHARSET "UTF-8"
#endif
#endif

#ifndef CABOCHA_DEFAULT_RC
#define CABOCHA_DEFAULT_RC "/usr/local/etc/cabocharc"
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WPATH(path) (CaboCha::Utf8ToWide(path).c_str())
#else
#define WPATH(path) (path)
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
typedef unsigned __int64   uint64;
typedef __int64             int64;
#else
typedef unsigned long long uint64;
typedef long long           int64;
#endif

namespace CaboCha {

enum { PARSING_MODE, TRAINING_MODE };

class die {
 public:
  die() {}
  ~die() {
    std::cerr << std::endl;
    exit(-1);
  }
  int operator&(std::ostream&) { return 0; }
};

struct whatlog {
  std::ostringstream stream_;
  std::string str_;
  const char *str() {
    str_ = stream_.str();
    return str_.c_str();
  }
};

class wlog {
 public:
  wlog(whatlog *what) : what_(what) {
    what_->stream_.clear();
  }
  bool operator&(std::ostream &) {
    return false;
  }
 private:
  whatlog *what_;
};
}  // CaboCha

#define WHAT what_.stream_

#define CHECK_FALSE(condition) \
 if (condition) {} else return \
   wlog(&what_) & what_.stream_ <<              \
      __FILE__ << "(" << __LINE__ << ") [" << #condition << "] "

#define CHECK_TREE_FALSE(condition) \
 if (condition) {} else return \
  wlog(tree->allocator()->mutable_what()) & \
    tree->allocator()->mutable_what()->stream_ << \
      __FILE__ << "(" << __LINE__ << ") [" << #condition << "] "

#define CHECK_DIE(condition) \
(condition) ? 0 : die() & std::cerr << __FILE__ << \
"(" << __LINE__ << ") [" << #condition << "] "

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <hash_map>
#include <hash_set>
#if _MSC_VER < 1310 || _MSC_VER >= 1600
using std::hash_map;
#else
using stdext::hash_map;
#endif
#else

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40300
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#define hash_map std::tr1::unordered_map
#define hash_set std::tr1::unordered_set
#else
#include <ext/hash_map>
#include <ext/hash_set>
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

#include <string>
namespace __gnu_cxx {
template <>
struct hash<std::string> {
  std::size_t operator()(const std::string &s) const {
    std::size_t result = 0;
    for (std::string::const_iterator it = s.begin(); it != s.end(); ++it) {
      result = (result * 131) + *it;
    }
    return result;
  }
};

template <>
struct hash<uint64> {
  std::size_t operator()(const uint64 s) const {
    return s;
  }
};
}
#endif
#endif
#endif
