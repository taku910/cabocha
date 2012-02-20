// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: tree_allocator.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_TREE_ALLOCATOR_H_
#define CABOCHA_TREE_ALLOCATOR_H_

#include <cstring>
#include <iostream>
#include <mecab.h>
#include <crfpp.h>
#include "cabocha.h"
#include "common.h"
#include "freelist.h"
#include "morph.h"
#include "string_buffer.h"

namespace CaboCha {

class TreeAllocator {
 public:
  std::vector<const Token *>  token;
  std::vector<const Chunk *>  chunk;
  std::string                 sentence;
  std::vector<const char *>   feature;

  mecab_lattice_t *mecab_lattice;
  crfpp_t         *crfpp_chunker;
  crfpp_t         *crfpp_ne;

  TreeAllocator():
      mecab_lattice(0),
      crfpp_chunker(0),
      crfpp_ne(0),
      char_freelist_(BUF_SIZE * 16),
      token_freelist_(CABOCHA_TOKEN_SIZE),
      chunk_freelist_(CABOCHA_CHUNK_SIZE),
      char_array_freelist_(BUF_SIZE),
      stream_(&std::cout) {}

  virtual ~TreeAllocator() {
    if (mecab_lattice) {
      MorphAnalyzer::deleteMeCabLattice(mecab_lattice);
      mecab_lattice = 0;
    }
    if (crfpp_chunker) {
      crfpp_destroy(crfpp_chunker);
      crfpp_chunker = 0;
    }
    if (crfpp_ne) {
      crfpp_destroy(crfpp_ne);
      crfpp_ne = 0;
    }
  }

  void free() {
    char_freelist_.free();
    token_freelist_.free();
    chunk_freelist_.free();
    char_array_freelist_.free();
    os_.clear();
    if (mecab_lattice) {
      MorphAnalyzer::clearMeCabLattice(mecab_lattice);
    }
    if (crfpp_chunker) {
      crfpp_clear(crfpp_chunker);
    }
    if (crfpp_ne) {
      crfpp_clear(crfpp_ne);
    }
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

  std::ostream *stream() const { return stream_; }
  void set_stream(std::ostream *os) {
    stream_ = os;
  }

 private:
  FreeList<char>   char_freelist_;
  FreeList<Token>  token_freelist_;
  FreeList<Chunk>  chunk_freelist_;
  FreeList<char *> char_array_freelist_;
  StringBuffer     os_;
  std::ostream    *stream_;
};
}
#endif
