// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: stream_wrapper.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_STREAM_WRAPPER_H_
#define CABOCHA_STREAM_WRAPPER_H_

#include <iostream>
#include <fstream>
#include <cstring>

namespace CaboCha {

  class istream_wrapper {
  private:
    std::istream* is_;
  public:
    std::istream &operator*() const  { return *is_; }
    std::istream *operator-> () const { return is_;  }
    explicit istream_wrapper(const char* filename): is_(0) {
      if (std::strcmp(filename, "-") == 0)
        is_ = &std::cin;
      else
        is_ = new std::ifstream(filename);
    }

    virtual ~istream_wrapper() {
      if (is_ != &std::cin) delete is_;
    }
    std::istream *get() { return is_; }
  };

  class ostream_wrapper {
  private:
    std::ostream* os_;
  public:
    std::ostream &operator*() const  { return *os_; }
    std::ostream *operator-> () const { return os_;  }
    explicit ostream_wrapper(const char* filename): os_(0) {
      if (std::strcmp(filename, "-") == 0)
        os_ = &std::cout;
      else
        os_ = new std::ofstream(filename);
    }

    std::ostream *get() { return os_; }

    virtual ~ostream_wrapper() {
      if (os_ != &std::cout) delete os_;
    }
  };
}

#endif
