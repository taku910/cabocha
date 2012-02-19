// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: morph.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <mecab.h>
#include "cabocha.h"
#include "param.h"
#include "common.h"
#include "morph.h"
#include "utils.h"

#if defined  (_WIN32) && !defined (__CYGWIN__)
namespace {
using namespace CaboCha;

typedef void* (*DLPROC)(...);
HINSTANCE g_mecab_handle;
DLPROC g_mecab_new;
DLPROC g_mecab_destroy;
DLPROC g_mecab_sparse_tonode2;
DLPROC g_mecab_strerror;
DLPROC g_mecab_dictionary_info;

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
  g_mecab_handle = LoadLibrary(WPATH(libmecabdll.c_str()));
  CHECK_DIE(g_mecab_handle != 0)
      << "LoadLibrary(\"" << libmecabdll << "\") failed";
  g_mecab_new             = (DLPROC)GetProcAddress(g_mecab_handle,
                                                   "mecab_new");
  g_mecab_destroy         = (DLPROC)GetProcAddress(g_mecab_handle,
                                                   "mecab_destroy");
  g_mecab_sparse_tonode2  = (DLPROC)GetProcAddress(g_mecab_handle,
                                                   "mecab_sparse_tonode2");
  g_mecab_strerror        = (DLPROC)GetProcAddress(g_mecab_handle,
                                                   "mecab_strerror");
  g_mecab_dictionary_info = (DLPROC)GetProcAddress(g_mecab_handle,
                                                   "mecab_dictionary_info");
  CHECK_DIE(g_mecab_new);
  CHECK_DIE(g_mecab_destroy);
  CHECK_DIE(g_mecab_sparse_tonode2);
  CHECK_DIE(g_mecab_strerror);
  CHECK_DIE(g_mecab_dictionary_info);
}

void mecab_detatch() {
  FreeLibrary(g_mecab_handle);
  g_mecab_handle = 0;
}

#define mecab_new_f             (mecab_t*)(*g_mecab_new)
#define mecab_destroy_f         (*g_mecab_destroy)
#define mecab_strerror_f        (const char*)(*g_mecab_strerror)
#define mecab_sparse_tonode2_f  (const mecab_node_t*)(*g_mecab_sparse_tonode2)
#define mecab_dictionary_info_f                                 \
  (const mecab_dictionary_info_t*)(*g_mecab_dictionary_info)

#else

#define mecab_new_f             mecab_new
#define mecab_destroy_f         mecab_destroy
#define mecab_strerror_f        mecab_strerror
#define mecab_sparse_tonode2_f  mecab_sparse_tonode2
#define mecab_dictionary_info_f mecab_dictionary_info

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

  const std::string mecabdic = param.get<std::string>("mecab-dicdir");
  if (!mecabdic.empty()) {
    argv.push_back("-d");
    argv.push_back(mecabdic.c_str());
  }

  mecab_ = mecab_new_f(argv.size(), const_cast<char **>(&argv[0]));
  CHECK_FALSE(mecab_) << mecab_strerror_f(0);

  const mecab_dictionary_info_t *dinfo =
      mecab_dictionary_info_f(mecab_);
  if (dinfo) {
    CHECK_FALSE(charset() ==
                decode_charset(dinfo->charset))
        << "Incompatible charset: MeCab charset is "
        << dinfo->charset
        << ", Your charset is " << encode_charset(charset());
  }

  return true;
}

void MorphAnalyzer::close() {
  mecab_destroy_f(mecab_);
  mecab_ = 0;
}

bool MorphAnalyzer::parse(Tree *tree) {
  CHECK_FALSE(tree);
  sentence_.assign(tree->sentence(), tree->sentence_size());
  const mecab_node_t *node  =
      mecab_sparse_tonode2_f(mecab_,
                             sentence_.c_str(),
                             sentence_.size());
  CHECK_FALSE(node) << mecab_strerror_f(mecab_);
  CHECK_FALSE(tree->read(node)) << "parse failed";
  tree->set_output_layer(OUTPUT_POS);
  return true;
};
}
