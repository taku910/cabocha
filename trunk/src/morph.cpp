// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: morph.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <mecab.h>
#include "cabocha.h"
#include "common.h"
#include "morph.h"
#include "param.h"
#include "tree_allocator.h"
#include "utils.h"

#if defined  (_WIN32) && !defined (__CYGWIN__)
namespace {
using namespace CaboCha;

typedef void* (*DLPROC)(...);
HINSTANCE g_mecab_handle;
DLPROC g_mecab_new;
DLPROC g_mecab_destroy;
DLPROC g_mecab_strerror;
DLPROC g_mecab_dictionary_info;
DLPROC g_mecab_parse_lattice;
DLPROC g_mecab_lattice_new;
DLPROC g_mecab_lattice_add_request_type;
DLPROC g_mecab_lattice_set_sentence2;
DLPROC g_mecab_lattice_strerror;
DLPROC g_mecab_lattice_get_bos_node;
DLPROC g_mecab_lattice_clear;
DLPROC g_mecab_lattice_destroy;

const std::string find_libmecab_dll() {
  std::string rcfile =
      get_windows_reg_value("software\\mecab", "mecabrc");
  if (!rcfile.empty()) {
    remove_filename(&rcfile);
    return rcfile + "\\..\\bin\\libmecab.dll";
  }
  return "libmecab.dll";
}
}

void mecab_attach() {
  using namespace CaboCha;
  const std::string libmecabdll = find_libmecab_dll();
  g_mecab_handle = ::LoadLibrary(WPATH(libmecabdll.c_str()));
  CHECK_DIE(g_mecab_handle != 0)
      << "LoadLibrary(\"" << libmecabdll << "\") failed";
  g_mecab_new
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_new");
  g_mecab_destroy
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_destroy");
  g_mecab_strerror
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_strerror");
  g_mecab_dictionary_info
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_dictionary_info");
  g_mecab_parse_lattice
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_parse_lattice");
  g_mecab_lattice_new
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_new");
  g_mecab_lattice_add_request_type
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_add_request_type");
  g_mecab_lattice_set_sentence2
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_set_sentence2");
  g_mecab_lattice_strerror
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_strerror");
  g_mecab_lattice_get_bos_node
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_get_bos_node");
  g_mecab_lattice_clear
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_clear");
  g_mecab_lattice_destroy
      = (DLPROC)::GetProcAddress(g_mecab_handle,
                                 "mecab_lattice_destroy");
  CHECK_DIE(g_mecab_new);
  CHECK_DIE(g_mecab_destroy);
  CHECK_DIE(g_mecab_strerror);
  CHECK_DIE(g_mecab_dictionary_info);
  CHECK_DIE(g_mecab_parse_lattice);
  CHECK_DIE(g_mecab_lattice_new);
  CHECK_DIE(g_mecab_lattice_add_request_type);
  CHECK_DIE(g_mecab_lattice_set_sentence2);
  CHECK_DIE(g_mecab_lattice_strerror);
  CHECK_DIE(g_mecab_lattice_get_bos_node);
  CHECK_DIE(g_mecab_lattice_clear);
  CHECK_DIE(g_mecab_lattice_destroy);
}

void mecab_detatch() {
  ::FreeLibrary(g_mecab_handle);
  g_mecab_handle = 0;
}

#define mecab_new_f \
  (mecab_t*)(*g_mecab_new)
#define mecab_destroy_f \
  (void)(*g_mecab_destroy)
#define mecab_strerror_f \
  (const char*)(*g_mecab_strerror)
#define mecab_dictionary_info_f                                 \
  (const mecab_dictionary_info_t*)(*g_mecab_dictionary_info)
#define mecab_parse_lattice_f \
  (int)(*g_mecab_parse_lattice)
#define mecab_lattice_new_f                     \
  (mecab_lattice_t*)(*g_mecab_lattice_new)
#define mecab_lattice_add_request_type_f        \
  (void)(*g_mecab_lattice_add_request_type)
#define mecab_lattice_set_sentence2_f          \
  (void)(*g_mecab_lattice_set_sentence2)
#define mecab_lattice_strerror_f \
  (const char*)(*g_mecab_lattice_strerror)
#define mecab_lattice_get_bos_node_f \
  (mecab_node_t*)(*g_mecab_lattice_get_bos_node)
#define mecab_lattice_clear_f  \
  (void)(*g_mecab_lattice_clear)
#define mecab_lattice_destroy_f  \
  (void)(*g_mecab_lattice_destroy)

#else

#define mecab_new_f                      mecab_new
#define mecab_destroy_f                  mecab_destroy
#define mecab_strerror_f                 mecab_strerror
#define mecab_dictionary_info_f          mecab_dictionary_info
#define mecab_parse_lattice_f            mecab_parse_lattice
#define mecab_lattice_new_f              mecab_lattice_new
#define mecab_lattice_add_request_type_f mecab_lattice_add_request_type
#define mecab_lattice_set_sentence2_f    mecab_lattice_set_sentence2
#define mecab_lattice_strerror_f         mecab_lattice_strerror
#define mecab_lattice_get_bos_node_f     mecab_lattice_get_bos_node
#define mecab_lattice_clear_f            mecab_lattice_clear
#define mecab_lattice_destroy_f          mecab_lattice_destroy

#endif

namespace CaboCha {

MorphAnalyzer::MorphAnalyzer(): mecab_(0) {}
MorphAnalyzer::~MorphAnalyzer() { close(); }

bool MorphAnalyzer::open(const Param &param) {
  CHECK_FALSE(action_mode() == PARSING_MODE)
      << "MorphAnalyzer supports PARSING_MODE only";

  const std::string mecabrc = param.get<std::string>("mecabrc");
  std::vector<const char *> argv;
  argv.push_back(param.program_name());
  if (!mecabrc.empty()) {
    argv.push_back("-r");
    argv.push_back(mecabrc.c_str());
  }

  const std::string mecabdic =
      param.get<std::string>("mecab-dicdir");
  if (!mecabdic.empty()) {
    argv.push_back("-d");
    argv.push_back(mecabdic.c_str());
  }

  mecab_ = mecab_new_f(argv.size(),
                       const_cast<char **>(&argv[0]));
  CHECK_FALSE(mecab_) << mecab_strerror_f(0);

  const mecab_dictionary_info_t *dinfo =
      mecab_dictionary_info_f(mecab_);
  CHECK_FALSE(dinfo) << "dictionary info is not available";
  CHECK_FALSE(charset() ==
              decode_charset(dinfo->charset))
      << "Incompatible charset: MeCab charset is "
      << dinfo->charset
      << ", Your charset is " << encode_charset(charset());

  return true;
}

void MorphAnalyzer::close() {
  mecab_destroy_f(mecab_);
  mecab_ = 0;
}

bool MorphAnalyzer::parse(Tree *tree) const {
  TreeAllocator *allocator = tree->allocator();
  CHECK_TREE_FALSE(allocator);

  if (!allocator->mecab_lattice) {
    allocator->mecab_lattice = mecab_lattice_new_f();
    CHECK_TREE_FALSE(allocator->mecab_lattice);
  }

  mecab_lattice_add_request_type_f(
      allocator->mecab_lattice,
      MECAB_ALLOCATE_SENTENCE);
  mecab_lattice_set_sentence2_f(
      allocator->mecab_lattice,
      tree->sentence(),
      tree->sentence_size());

  CHECK_TREE_FALSE(mecab_parse_lattice_f(
                       mecab_, allocator->mecab_lattice))
      << mecab_lattice_strerror_f(allocator->mecab_lattice);

  const mecab_node_t *node  =
      mecab_lattice_get_bos_node_f(allocator->mecab_lattice);
  CHECK_TREE_FALSE(node);
  CHECK_TREE_FALSE(tree->read(node)) << "parse failed";
  tree->set_output_layer(OUTPUT_POS);
  return true;
};

// static
void MorphAnalyzer::clearMeCabLattice(mecab_lattice_t *lattice) {
  mecab_lattice_clear_f(lattice);
}

// static
void MorphAnalyzer::deleteMeCabLattice(mecab_lattice_t *lattice) {
  mecab_lattice_destroy_f(lattice);
}
}
