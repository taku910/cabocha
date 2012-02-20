// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: tree_allocator.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <mecab.h>
#include <crfpp.h>
#include <cstring>
#include <iostream>
#include "cabocha.h"
#include "common.h"
#include "dep.h"
#include "freelist.h"
#include "morph.h"
#include "string_buffer.h"
#include "tree_allocator.h"

namespace CaboCha {

TreeAllocator::TreeAllocator()
    : mecab_lattice(0),
      crfpp_chunker(0),
      crfpp_ne(0),
      dependency_parser_data(0),
      char_freelist_(BUF_SIZE * 16),
      token_freelist_(CABOCHA_TOKEN_SIZE),
      chunk_freelist_(CABOCHA_CHUNK_SIZE),
      char_array_freelist_(BUF_SIZE),
      stream_(&std::cout) {}

TreeAllocator::~TreeAllocator() {
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
  delete dependency_parser_data;
  dependency_parser_data = 0;
}

void TreeAllocator::free() {
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

void TreeAllocator::clear() {
  return this->free();
}

char *TreeAllocator::alloc(size_t size) {
  return char_freelist_.alloc(size + 1);
}

char *TreeAllocator::alloc(const char *str) {
  const size_t size = std::strlen(str);
  char *n = char_freelist_.alloc(size + 1);
  std::strncpy(n, str, size + 1);
  return n;
}

char **TreeAllocator::alloc_char_array(size_t size) {
  return char_array_freelist_.alloc(size);
}

Chunk* TreeAllocator::allocChunk() {
  Chunk *c = chunk_freelist_.alloc();
  std::memset(c, 0, sizeof(Chunk));
  c->link = -1;
  return c;
}

Token* TreeAllocator::allocToken() {
  Token *t = token_freelist_.alloc();
  std::memset(t, 0, sizeof(Token));
  return t;
}

StringBuffer *TreeAllocator::mutable_string_buffer() {
  return &os_;
}

std::ostream *TreeAllocator::stream() const {
  return stream_;
}

void TreeAllocator::set_stream(std::ostream *os) {
  stream_ = os;
}

whatlog *TreeAllocator::mutable_what() {
  return &what_;
}

const char *TreeAllocator::what() {
  return what_.str();
}
}
