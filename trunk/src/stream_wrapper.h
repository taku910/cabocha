// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: stream_wrapper.h 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_STREAM_WRAPPER_H_
#define CABOCHA_STREAM_WRAPPER_H_

#include <iostream>
#include <fstream>
#include <cstring>
#include "common.h"

namespace CaboCha {

class istream_wrapper {
 private:
  std::istream* is;
 public:
  std::istream &operator*() const  { return *is; }
  std::istream *operator->() const { return is;  }
  std::istream *get() { return is; }
  explicit istream_wrapper(const char* filename): is(0) {
    if (std::strcmp(filename, "-") == 0)
      is = &std::cin;
    else
      is = new std::ifstream(WPATH(filename));
  }

  ~istream_wrapper() {
    if (is != &std::cin) delete is;
  }
};

class ostream_wrapper {
 private:
  std::ostream* os;
 public:
  std::ostream &operator*() const  { return *os; }
  std::ostream *operator->() const { return os;  }
  std::ostream *get() { return os; }
  explicit ostream_wrapper(const char* filename): os(0) {
    if (std::strcmp(filename, "-") == 0)
      os = &std::cout;
    else
      os = new std::ofstream(WPATH(filename));
  }

  ~ostream_wrapper() {
    if (os != &std::cout) delete os;
  }
};
}

#endif
