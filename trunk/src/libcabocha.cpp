// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: libcabocha.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#endif
#include <crfpp.h>
#include <string>
#include <fstream>
#include <iostream>
#include "cabocha.h"
#include "dep.h"
#include "ne.h"
#include "chunker.h"
#include "utils.h"
#include "ucs.h"
#include "param.h"
#include "svm.h"
#include "common.h"

namespace {
const int LIBCABOCHA_ID = 5220707;
}  // namespace

namespace CaboCha {

std::string *getStaticErrorString() {
  static std::string errorStr;
  return &errorStr;
}

const char *getGlobalError() {
  return getStaticErrorString()->c_str();
}

void setGlobalError(const char *str) {
  std::string *error = getStaticErrorString();
  *error = str;
}
}

#if defined(_WIN32) && !defined(__CYGWIN__)
#ifdef __cplusplus
void mecab_attach();
void mecab_detatch();
extern "C" {
#endif
  HINSTANCE DllInstance = 0;
  BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, void*) {
    if (!DllInstance) {
      DllInstance = hinst;
    }
    if (dwReason == DLL_PROCESS_ATTACH) {
      mecab_attach();
    } else if (dwReason == DLL_PROCESS_DETACH) {
      mecab_detatch();
    }
    return TRUE;
  }
#ifdef __cplusplus
}
#endif
#endif

struct cabocha_t {
  int allocated;
  CaboCha::Parser *ptr;
};

cabocha_t* cabocha_new(int argc, char **argv) {
  cabocha_t *c = new cabocha_t;
  CaboCha::Parser *ptr = CaboCha::createParser(argc, argv);
  if (!c || !ptr) {
    delete c;
    delete ptr;
    CaboCha::setGlobalError(CaboCha::getParserError());
    return 0;
  }
  c->ptr = ptr;
  c->allocated = LIBCABOCHA_ID;
  return c;
}

cabocha_t* cabocha_new2(const char *arg) {
  cabocha_t *c = new cabocha_t;
  CaboCha::Parser *ptr = CaboCha::createParser(arg);
  if (!c || !ptr) {
    delete c;
    delete ptr;
    CaboCha::setGlobalError(CaboCha::getParserError());
    return 0;
  }
  c->ptr = ptr;
  c->allocated = LIBCABOCHA_ID;
  return c;
}

const char *cabocha_version() {
  return CaboCha::Parser::version();
}

const char* cabocha_strerror(cabocha_t *c) {
  if (!c || !c->allocated) return CaboCha::getGlobalError();
  return c->ptr->what();
}

void cabocha_destroy(cabocha_t *c) {
  if (c && c->allocated) {
    delete c->ptr;
    delete c;
  }
  c = 0;
}

#define CABOCHA_CHECK_FIRST_ARG(c, t)                           \
  if (!(c) || (c)->allocated != LIBCABOCHA_ID) {                \
    CaboCha::setGlobalError("first argment seems invalid");     \
    return 0;                                                   \
  } CaboCha::Parser *(t) = (c)->ptr;

const char *cabocha_sparse_tostr(cabocha_t *c, const char *s) {
  CABOCHA_CHECK_FIRST_ARG(c, t);
  return const_cast<char*>(t->parseToString(s));
}

const char *cabocha_sparse_tostr2(cabocha_t *c, const char *s, size_t len) {
  CABOCHA_CHECK_FIRST_ARG(c, t);
  return const_cast<char*>(t->parseToString(s, len));
}

const char *cabocha_sparse_tostr3(cabocha_t *c, const char *s, size_t len,
                                  char *s2,  size_t len2) {
  CABOCHA_CHECK_FIRST_ARG(c, t);
  return const_cast<char*>(t->parseToString(s, len, s2, len2));
}

const cabocha_tree_t *cabocha_sparse_totree(cabocha_t* c, const char *s) {
  CABOCHA_CHECK_FIRST_ARG(c, t);
  return reinterpret_cast<const cabocha_tree_t *> (t->parse(s));
}

const cabocha_tree_t *cabocha_sparse_totree2(cabocha_t* c,
                                             const char *s, size_t len) {
  CABOCHA_CHECK_FIRST_ARG(c, t);
  return reinterpret_cast<const cabocha_tree_t *> (t->parse(s, len));
}

cabocha_tree_t *cabocha_tree_new() {
  CaboCha::Tree *t = new CaboCha::Tree;
  return reinterpret_cast<cabocha_tree_t *>(t);
}
void cabocha_tree_destroy(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  delete t2;
  t2 = 0;
}

int cabocha_tree_empty(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return static_cast<int>(t2->empty());
}

void cabocha_tree_clear(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->clear();
}

void cabocha_tree_clear_chunk(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->clear_chunk();
}

size_t cabocha_tree_size(cabocha_tree_t* t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *> (t);
  return t2->size();
}

size_t cabocha_tree_chunk_size(cabocha_tree_t* t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *> (t);
  return t2->chunk_size();
}

size_t cabocha_tree_token_size(cabocha_tree_t* t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *> (t);
  return t2->token_size();
}

const char *cabocha_tree_sentence(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->sentence();
}

size_t cabocha_tree_sentence_size(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->sentence_size();
}

void cabocha_tree_set_sentence(cabocha_tree_t *t,
                               const char *sentence,
                               size_t length) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->set_sentence(sentence, length);
}

int cabocha_tree_read(cabocha_tree_t *t ,
                      const char *input,
                      size_t length,
                      cabocha_input_layer_t input_layer) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->read(input, length, input_layer);
}

int cabocha_tree_read_from_mecab_node(cabocha_tree_t *t ,
                                      const mecab_node_t *node) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->read(node);
}

const cabocha_token_t *cabocha_tree_token(cabocha_tree_t *t, size_t i) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return reinterpret_cast<const cabocha_token_t *>(t2->token(i));
}

const cabocha_chunk_t *cabocha_tree_chunk(cabocha_tree_t *t, size_t i) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return reinterpret_cast<const cabocha_chunk_t *>(t2->chunk(i));
}

cabocha_token_t *cabocha_tree_add_token(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return reinterpret_cast<cabocha_token_t *>(t2->add_token());
}

cabocha_chunk_t *cabocha_tree_add_chunk(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return reinterpret_cast<cabocha_chunk_t *>(t2->add_chunk());
}

char            *cabocha_tree_strdup(cabocha_tree_t* t, const char *str) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->strdup(str);
}

char            *cabocha_tree_alloc(cabocha_tree_t* t, size_t size) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->alloc(size);
}

const char *cabocha_tree_tostr(cabocha_tree_t* t, cabocha_format_t fmt) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->toString(fmt);
}

const char *cabocha_tree_tostr2(cabocha_tree_t* t, cabocha_format_t fmt,
                                char *out, size_t len) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->toString(fmt, out, len);
}

void cabocha_tree_set_charset(cabocha_tree_t* t,
                              cabocha_charset_t charset) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->set_charset(charset);
}

cabocha_charset_t cabocha_tree_charset(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->charset();
}

void cabocha_tree_set_posset(cabocha_tree_t *t,
                             cabocha_posset_t posset) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->set_posset(posset);
}

cabocha_posset_t cabocha_tree_posset(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->posset();
}

void cabocha_tree_set_output_layer(cabocha_tree_t *t,
                                   cabocha_output_layer_t output_layer) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->set_output_layer(output_layer);
}

cabocha_output_layer_t cabocha_tree_output_layer(cabocha_tree_t *t) {
  CaboCha::Tree* t2 = reinterpret_cast<CaboCha::Tree *>(t);
  return t2->output_layer();
}

