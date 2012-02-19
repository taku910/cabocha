// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: ne.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <crfpp.h>
#include <vector>
#include <string>
#include <cstdio>
#include <iterator>
#include "cabocha.h"
#include "ne.h"
#include "param.h"
#include "ucs.h"
#include "utils.h"
#include "char_category.h"

namespace CaboCha {
namespace {

int get_char_class(int charset,
                   const char *begin,
                   const char *end, size_t *mblen) {
  unsigned short int c = 0;
  switch (charset) {
#ifndef CABOCHA_USE_UTF8_ONLY
    case EUC_JP: c = euc_to_ucs2(begin, end, mblen); break;
    case CP932:  c = cp932_to_ucs2(begin, end, mblen); break;
#endif
    case ASCII:  c = utf8_to_ucs2(begin, end, mblen); break;
    case UTF8:   c = utf8_to_ucs2(begin, end, mblen); break;
    default:     c = utf8_to_ucs2(begin, end, mblen); break;
  }
  return char_category[c];
}

void get_char_feature(int charset, const char *str, char *feature) {
  const char *end = str + std::strlen(str);
  int iscap = false;
  int type1 = -1;
  int type2 = -1;
  size_t mblen = 0;
  size_t clen = 0;

  while (str < end) {
    int c = get_char_class(charset, str, end,  &mblen);
    str += mblen;
    if (iscap && c != ALPHA) iscap = false;
    if (clen == 0) {
      type1 = c;
    } else if (clen == 1 && type1 == CAPALPHA && c == ALPHA) {
      type2 = type1;
      type1 = c;
      iscap = true;
    } else if (c == HYPHEN) {
    } else if (c != type1) {
      type2 = type1;
      type1 = c;
    }

    ++clen;
  }

  if (iscap) {
    snprintf(feature, sizeof(feature), "C");
  } else if (clen == 1) {
    snprintf(feature, sizeof(feature), "S%d", type1);
  } else if (type2 == -1) {
    snprintf(feature, sizeof(feature), "M%d",  type1);
  } else {
    snprintf(feature, sizeof(feature), "T%d-%d", type2, type1);
  }
}
} // end of anonmyous

NE::NE(): tagger_(0) {}
NE::~NE() { close(); }

bool NE::open(const Param &param) {
  close();

  if (action_mode() == PARSING_MODE) {
    const std::string filename = param.get<std::string>("ne-model");
    std::vector<const char*> argv;
    argv.push_back(param.program_name());
    argv.push_back("-m");
    argv.push_back(filename.c_str());
    tagger_ = crfpp_new(argv.size(),
                        const_cast<char **>(&argv[0]));
    CHECK_FALSE(tagger_) << crfpp_strerror(tagger_);
    CHECK_FALSE(crfpp_ysize(tagger_) >= 2);
    CHECK_FALSE(crfpp_xsize(tagger_) == 3);
    for (size_t i = 0; i < crfpp_ysize(tagger_); ++i) {
      const char *p = crfpp_yname(tagger_, i);
      CHECK_FALSE(p && (p[0] == 'B' || p[0] == 'I' || p[0] == 'O'));
    }
  }

  return true;
}

void NE::close() {
  crfpp_destroy(tagger_);
  ne_composite_.clear();
  tagger_ = 0;
}

bool NE::parse(Tree *tree) {
  CHECK_FALSE(tree);

  if (ne_composite_.empty()) {
    switch (tree->posset()) {
      case IPA:
        // "名詞,数,"
        ne_composite_ = "\xE5\x90\x8D\xE8\xA9\x9E,\xE6\x95\xB0,";
        break;
      case JUMAN:
        // 名詞,数詞
        ne_composite_ = "\xE5\x90\x8D\xE8\xA9\x9E,\xE6\x95\xB0\xE8\xA9\x9E,";
        break;
      case UNIDIC:
        // 名詞,数詞
        ne_composite_ = "\xE5\x90\x8D\xE8\xA9\x9E,\xE6\x95\xB0\xE8\xA9\x9E,";
      default:
        break;
    }
    Iconv iconv;
    iconv.open(UTF8, charset());
    CHECK_DIE(iconv.convert(&ne_composite_));
    CHECK_FALSE(!ne_composite_.empty());
  }

  if (action_mode() == PARSING_MODE) {
    CHECK_FALSE(tagger_);
    crfpp_clear(tagger_);
  }

  int comp = 0;
  const size_t size  = tree->token_size();
  std::string tmp;
  for (size_t i = 0; i < size; ++i) {
    const Token *token = tree->token(i);
    const char *surface = token->normalized_surface;
    const char *feature = token->feature;
    if (std::string(feature).find(ne_composite_) == 0) {
      ++comp;
    } else {
      comp = 0;
    }

    if (comp >= 2) continue;

    char *char_feature = tree->alloc(16);
    get_char_feature(charset(), surface, char_feature);
    if (tree->posset() == IPA || tree->posset() == UNIDIC) {
      concat_feature(token, 4, &tmp);
    } else if (tree->posset() == JUMAN) {
      concat_feature(token, 2, &tmp);
    }
    const char *pos = tree->strdup(tmp.c_str());
    feature_.clear();
    feature_.push_back(surface);
    feature_.push_back(char_feature);
    feature_.push_back(pos);

    const char *ne = token->ne;
    if (action_mode() == PARSING_MODE) {
      if (ne) feature_.push_back(ne);
      crfpp_add2(tagger_, feature_.size(), (const char **)&feature_[0]);
    } else {
      CHECK_FALSE(ne) << "named entity is not defined";
      std::copy(feature_.begin(), feature_.end(),
                std::ostream_iterator<const char*>(*stream(), " "));
      *stream() << ne << std::endl;
    }
  }

  if (action_mode() == PARSING_MODE) {
    CHECK_FALSE(crfpp_parse(tagger_));
    const char *prev = 0;
    size_t      ci   = 0;
    int         comp = 0;

    for (size_t i = 0; i < size; ++i) {
      Token *token = tree->mutable_token(i);
      if (std::string(token->feature).find(ne_composite_) == 0)
        ++comp;
      else
        comp = 0;

      if (comp >= 2) {
        char *ne = tree->strdup(prev);
        if (ne[0] != 'O') ne[0] = 'I';
        token->ne = ne;
      } else {
        token->ne = tree->strdup(crfpp_y2(tagger_, ci));
        ++ci;
      }

      if ((prev && prev[0] == 'I' &&
           token->ne[0] == 'I' && std::strlen(token->ne) >= 3 &&
           std::strlen(prev) >= 3 && std::strcmp(token->ne + 3, prev + 3)) ||
          (i == 0 && token->ne[0] == 'I')) {
        char *ne = tree->strdup(token->ne);
        ne[0] = 'B';
        token->ne = ne;
      }

      prev = token->ne;
    }
  } else {
    *stream() << std::endl;
  }

  return true;
}
}
