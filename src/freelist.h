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

template <class T> class FreeList {
 private:
  std::vector<std::pair<size_t, T *> > block_;
  size_t current_index_;
  size_t block_index_;
  size_t default_size_;

 public:
  void free() {
    current_index_ = 0;
    block_index_ = 0;
  }

  T* alloc(size_t requested_size = 1) {
    while (block_index_ < block_.size()) {
      if ((current_index_ + requested_size) < block_[block_index_].first) {
        T *result = block_[block_index_].second + current_index_;
        current_index_ += requested_size;
        return result;
      }
      ++block_index_;
      current_index_ = 0;
    }

    const size_t alloc_size = std::max(requested_size, default_size_);
    block_.push_back(std::make_pair(alloc_size, new T[alloc_size]));

    block_index_ = block_.size() - 1;
    current_index_ += requested_size;

    return block_[block_index_].second;
  }

  explicit FreeList(size_t size) : current_index_(0),
                                   block_index_(0),
                                   default_size_(size) {}

  virtual ~FreeList() {
    for (size_t i = 0; i < block_.size(); ++i) {
      delete [] block_[i].second;
    }
  }
};
}
#endif
