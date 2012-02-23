// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: tree_allocator.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_TREE_ALLOCATOR_H_
#define CABOCHA_TREE_ALLOCATOR_H_

#include <cstring>
#include <iostream>
#include "cabocha.h"
#include "common.h"
#include "freelist.h"
#include "morph.h"
#include "scoped_ptr.h"
#include "string_buffer.h"

struct mecab_lattice_t;
struct crfpp_t;

namespace CaboCha {

class StringBuffer;
struct DependencyParserData;

class TreeAllocator {
 public:
  std::vector<const Token *>  token;
  std::vector<const Chunk *>  chunk;
  std::string                 sentence;
  std::vector<const char *>   feature;

  mecab_lattice_t      *mecab_lattice;
  crfpp_t              *crfpp_chunker;
  crfpp_t              *crfpp_ne;
  DependencyParserData *dependency_parser_data;

  TreeAllocator();
  virtual ~TreeAllocator();

  void free();
  void clear();

  char *alloc(size_t size);
  char *alloc(const char *str);
  char **alloc_char_array(size_t size);
  Chunk* allocChunk();
  Token* allocToken();
  StringBuffer *mutable_string_buffer();

  std::ostream *stream() const;
  void set_stream(std::ostream *os);

  whatlog *mutable_what();
  const char *what();

 private:
  FreeList<char>            char_freelist_;
  FreeList<Token>           token_freelist_;
  FreeList<Chunk>           chunk_freelist_;
  FreeList<char *>          char_array_freelist_;
  scoped_ptr<StringBuffer>  os_;
  std::ostream             *stream_;
  whatlog                   what_;
};
}
#endif
