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
#include "common.h"
#include "dep.h"
#include "ne.h"
#include "chunker.h"
#include "param.h"
#include "svm.h"
#include "ucs.h"
#include "utils.h"

namespace {
const char kUnknownError[] = "Unknown Error";
const size_t kErrorBufferSize = 256;
}  // namespace

#if defined(_WIN32) && !defined(__CYGWIN__)
void mecab_attach();
void mecab_detatch();

namespace {
DWORD g_tls_index = TLS_OUT_OF_INDEXES;
}  // namespace

namespace CaboCha {
const char *getGlobalError() {
  LPVOID data = ::TlsGetValue(g_tls_index);
  return data == NULL ? kUnknownError : reinterpret_cast<const char *>(data);
}

void setGlobalError(const char *str) {
  char *data = reinterpret_cast<char *>(::TlsGetValue(g_tls_index));
  if (data == NULL) {
    return;
  }
  strncpy(data, str, kErrorBufferSize - 1);
  data[kErrorBufferSize - 1] = '\0';
}
}  // CaboCha

HINSTANCE DllInstance = 0;

extern "C" {
  BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID) {
    LPVOID data = 0;
    if (!DllInstance) {
      DllInstance = hinst;
    }
    switch (dwReason) {
      case DLL_PROCESS_ATTACH:
        mecab_attach();
        if ((g_tls_index = ::TlsAlloc()) == TLS_OUT_OF_INDEXES) {
          return FALSE;
        }
        // Not break in order to initialize the TLS.
      case DLL_THREAD_ATTACH:
        data = (LPVOID)::LocalAlloc(LPTR, kErrorBufferSize);
        if (data) {
          ::TlsSetValue(g_tls_index, data);
        }
        break;
      case DLL_THREAD_DETACH:
        data = ::TlsGetValue(g_tls_index);
        if (data) {
          ::LocalFree((HLOCAL)data);
        }
        break;
      case DLL_PROCESS_DETACH:
        mecab_detatch();
        data = ::TlsGetValue(g_tls_index);
        if (data) {
          ::LocalFree((HLOCAL)data);
        }
        ::TlsFree(g_tls_index);
        g_tls_index = TLS_OUT_OF_INDEXES;
        break;
      default:
        break;
    }
    return TRUE;
  }
}  // extern "C"
#else  // _WIN32
namespace {
#ifdef HAVE_TLS_KEYWORD
__thread char kErrorBuffer[kErrorBufferSize];
#else
char kErrorBuffer[kErrorBufferSize];
#endif
}  // namespace

namespace CaboCha {
const char *getGlobalError() {
  return kErrorBuffer;
}

void setGlobalError(const char *str) {
  strncpy(kErrorBuffer, str, kErrorBufferSize - 1);
  kErrorBuffer[kErrorBufferSize - 1] = '\0';
}
}  // CaboCha
#endif  // _WIN32

cabocha_t* cabocha_new(int argc, char **argv) {
  CaboCha::Parser *ptr = CaboCha::createParser(argc, argv);
  if (!ptr) {
    CaboCha::setGlobalError(CaboCha::getParserError());
    return 0;
  }
  return reinterpret_cast<cabocha_t *>(ptr);
}

cabocha_t* cabocha_new2(const char *arg) {
  CaboCha::Parser *ptr = CaboCha::createParser(arg);
  if (!ptr) {
    CaboCha::setGlobalError(CaboCha::getParserError());
    return 0;
  }
  return reinterpret_cast<cabocha_t *>(ptr);
}

const char *cabocha_version() {
  return CaboCha::Parser::version();
}

const char* cabocha_strerror(cabocha_t *c) {
  if (!c) {
    return CaboCha::getGlobalError();
  }
  return reinterpret_cast<CaboCha::Parser *>(c)->what();
}

void cabocha_destroy(cabocha_t *c) {
  CaboCha::Parser *parser = reinterpret_cast<CaboCha::Parser *>(c);
  delete parser;
}

const char *cabocha_sparse_tostr(cabocha_t *c, const char *s) {
  return const_cast<char*>(
      reinterpret_cast<CaboCha::Parser *>(c)->parseToString(s));
}

const cabocha_tree_t  *cabocha_parse_tree(cabocha_t *c,
                                          cabocha_tree_t *tree) {
  return reinterpret_cast<const cabocha_tree_t *>(
      reinterpret_cast<CaboCha::Parser *>(c)->parse(
          reinterpret_cast<CaboCha::Tree *>(tree)));
}

const char *cabocha_sparse_tostr2(cabocha_t *c, const char *s, size_t len) {
  return const_cast<char*>(
      reinterpret_cast<CaboCha::Parser *>(c)->parseToString(s, len));
}

const char *cabocha_sparse_tostr3(cabocha_t *c, const char *s, size_t len,
                                  char *s2,  size_t len2) {
  return const_cast<char*>(
      reinterpret_cast<CaboCha::Parser *>(c)->parseToString(s, len, s2, len2));
}

const cabocha_tree_t *cabocha_sparse_totree(cabocha_t* c, const char *s) {
  return reinterpret_cast<const cabocha_tree_t *>(
      reinterpret_cast<CaboCha::Parser *>(c)->parse(s));
}

const cabocha_tree_t *cabocha_sparse_totree2(cabocha_t* c,
                                             const char *s, size_t len) {
  return reinterpret_cast<const cabocha_tree_t *>(
      reinterpret_cast<CaboCha::Parser *>(c)->parse(s, len));
}

cabocha_tree_t *cabocha_tree_new() {
  CaboCha::Tree *t = new CaboCha::Tree;
  return reinterpret_cast<cabocha_tree_t *>(t);
}

void cabocha_tree_destroy(cabocha_tree_t *t) {
  CaboCha::Tree* tree = reinterpret_cast<CaboCha::Tree *>(t);
  delete tree;
}

int cabocha_tree_empty(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->empty();
}

void cabocha_tree_clear(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->clear();;
}

void cabocha_tree_clear_chunk(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->clear_chunk();
}

size_t cabocha_tree_size(cabocha_tree_t* t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->size();
}

size_t cabocha_tree_chunk_size(cabocha_tree_t* t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->chunk_size();
}

size_t cabocha_tree_token_size(cabocha_tree_t* t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->token_size();
}

const char *cabocha_tree_sentence(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->sentence();
}

size_t cabocha_tree_sentence_size(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->sentence_size();
}

void cabocha_tree_set_sentence(cabocha_tree_t *t,
                               const char *sentence,
                               size_t length) {
  return reinterpret_cast<CaboCha::Tree *>(t)->set_sentence(sentence, length);
}

int cabocha_tree_read(cabocha_tree_t *t ,
                      const char *input,
                      size_t length,
                      cabocha_input_layer_t input_layer) {
  return reinterpret_cast<CaboCha::Tree *>(t)->read(input, length, input_layer);
}

int cabocha_tree_read_from_mecab_node(cabocha_tree_t *t ,
                                      const mecab_node_t *node) {
  return reinterpret_cast<CaboCha::Tree *>(t)->read(node);
}

const cabocha_token_t *cabocha_tree_token(cabocha_tree_t *t, size_t i) {
  return reinterpret_cast<const cabocha_token_t *>(
      reinterpret_cast<CaboCha::Tree *>(t)->token(i));
}

const cabocha_chunk_t *cabocha_tree_chunk(cabocha_tree_t *t, size_t i) {
  return reinterpret_cast<const cabocha_chunk_t *>(
      reinterpret_cast<CaboCha::Tree *>(t)->chunk(i));
}

cabocha_token_t *cabocha_tree_add_token(cabocha_tree_t *t) {
  return reinterpret_cast<cabocha_token_t *>(
      reinterpret_cast<CaboCha::Tree *>(t)->add_token());
}

cabocha_chunk_t *cabocha_tree_add_chunk(cabocha_tree_t *t) {
  return reinterpret_cast<cabocha_chunk_t *>(
      reinterpret_cast<CaboCha::Tree *>(t)->add_chunk());
}

char            *cabocha_tree_strdup(cabocha_tree_t* t, const char *str) {
  return reinterpret_cast<CaboCha::Tree *>(t)->strdup(str);
}

char            *cabocha_tree_alloc(cabocha_tree_t* t, size_t size) {
  return reinterpret_cast<CaboCha::Tree *>(t)->alloc(size);
}

const char *cabocha_tree_tostr(cabocha_tree_t* t, cabocha_format_t fmt) {
  return reinterpret_cast<CaboCha::Tree *>(t)->toString(fmt);
}

const char *cabocha_tree_tostr2(cabocha_tree_t* t, cabocha_format_t fmt,
                                char *out, size_t len) {
  return reinterpret_cast<CaboCha::Tree *>(t)->toString(fmt, out, len);
}

void cabocha_tree_set_charset(cabocha_tree_t* t,
                              cabocha_charset_t charset) {
  return reinterpret_cast<CaboCha::Tree *>(t)->set_charset(charset);
}

cabocha_charset_t cabocha_tree_charset(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->charset();
}

void cabocha_tree_set_posset(cabocha_tree_t *t,
                             cabocha_posset_t posset) {
  return reinterpret_cast<CaboCha::Tree *>(t)->set_posset(posset);
}

cabocha_posset_t cabocha_tree_posset(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->posset();
}

void cabocha_tree_set_output_layer(cabocha_tree_t *t,
                                   cabocha_output_layer_t output_layer) {
  return reinterpret_cast<CaboCha::Tree *>(t)->set_output_layer(output_layer);
}

cabocha_output_layer_t cabocha_tree_output_layer(cabocha_tree_t *t) {
  return reinterpret_cast<CaboCha::Tree *>(t)->output_layer();
}
