// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: freelist.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_FREELIST_H_
#define CABOCHA_FREELIST_H_

#include <iostream>
#include <vector>
#include <cstring>

namespace CaboCha {

  template <class T>
  class FreeList {
  private:
    std::vector <T *> freelist_;
    size_t pi_;
    size_t li_;
    size_t size_;

  public:
    void free() { li_ = pi_ = 0; }

    T* alloc(size_t len = 1) {
      if ((pi_ + len) >= size_) { li_++; pi_ = 0; }
      if (li_ == freelist_.size()) {
        freelist_.push_back(new T [size_]);
      }
      T* r = freelist_[li_] + pi_;
      pi_ += len;
      return r;
    }

    void set_size(size_t n) { size_ = n; }
    explicit FreeList(size_t size): pi_(0), li_(0), size_(size) {}
    FreeList(): pi_(0), li_(0), size_(0) {}
    ~FreeList() {
      for (size_t i = 0; i < freelist_.size(); ++i)
        delete [] freelist_[i];
    }
  };
}
#endif
