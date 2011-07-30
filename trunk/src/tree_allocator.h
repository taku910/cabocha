// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: tree_allocator.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_NE_H_
#define CABOCHA_NE_H_

#include <cstring>
#include "common.h"
#include "cabocha.h"
#include "freelist.h"
#include "string_buffer.h"

namespace CaboCha {

class TreeAllocator {
 private:
  FreeList<char>   char_freelist_;
  FreeList<Token>  token_freelist_;
  FreeList<Chunk>  chunk_freelist_;
  FreeList<char *> char_array_freelist_;
  StringBuffer     os_;

 public:
  std::vector<const Token *>  token;
  std::vector<const Chunk *>  chunk;
  std::string                 sentence;

  TreeAllocator():
      char_freelist_(BUF_SIZE * 16),
      token_freelist_(CABOCHA_TOKEN_SIZE),
      chunk_freelist_(CABOCHA_CHUNK_SIZE),
      char_array_freelist_(BUF_SIZE) {}

  void free() {
    char_freelist_.free();
    token_freelist_.free();
    chunk_freelist_.free();
    char_array_freelist_.free();
    os_.clear();
  }

  void clear() { return this->free(); }

  char *alloc(size_t size) {
    return char_freelist_.alloc(size + 1);
  }

  char *alloc(const char *str) {
    const size_t size = std::strlen(str);
    char *n = char_freelist_.alloc(size + 1);
    std::strcpy(n, str);
    return n;
  }

  char **alloc_char_array(size_t size) {
    return char_array_freelist_.alloc(size);
  }

  Chunk* allocChunk() {
    Chunk *c = chunk_freelist_.alloc();
    std::memset(c, 0, sizeof(Chunk));
    c->link = -1;
    return c;
  }

  Token* allocToken() {
    Token *t = token_freelist_.alloc();
    std::memset(t, 0, sizeof(Token));
    return t;
  }

  StringBuffer *mutable_string_buffer() { return &os_; }
};
}
#endif
