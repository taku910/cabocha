// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: selector.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <cstring>
#include <strstream>
#include "cabocha.h"
#include "common.h"
#include "scoped_ptr.h"
#include "selector.h"
#include "selector_pat.h"
#include "ucs.h"
#include "utils.h"

namespace CaboCha {
inline const char *getToken(const Token *token, size_t id) {
  if (token->feature_list_size <= id) {
    return 0;
  }
  if (std::strcmp("*", token->feature_list[id]) == 0) {
    return 0;
  }
  return token->feature_list[id];
}

inline void emitTokenFeatures(const char* header,
                              const Token *token,
                              size_t pos_size,
                              std::ostrstream *ostrs) {
  const char *surface = token->normalized_surface;
  const char *cform = getToken(token, pos_size + 1);

  *ostrs << ' ' << header << "S:" << surface;

  const size_t size =
      std::min(pos_size,
               static_cast<size_t>(token->feature_list_size));
  for (size_t k = 0; k < size; ++k) {
    if (std::strcmp("*", token->feature_list[k]) == 0) {
      break;
    }
    *ostrs << ' ' << header << 'P' << k << ':' << token->feature_list[k];
  }

  if (cform) {
    *ostrs << ' ' << header <<  "F:" << cform;
  }
}

PatternMatcher::PatternMatcher() : matched_result_(true) {}
PatternMatcher::~PatternMatcher() {}

void PatternMatcher::clear() {
  matched_result_ = true;
  patterns_.clear();
}

bool PatternMatcher::compile(const char *pattern,
                             Iconv *iconv) {
  clear();
  if (pattern[0] == '!') {
    matched_result_ = false;
    ++pattern;
  }

  std::string converted(pattern);
  if (iconv) {
    if (!iconv->convert(&converted)) {
      std::cerr << "cannot convert: " << pattern << std::endl;
    }
  }
  const size_t len = converted.size();
  const char *pat = converted.c_str();
  if (len >= 3 && pat[0] == '(' && pat[len-1] == ')') {
    scoped_fixed_array<char, BUF_SIZE> buf;
    CHECK_DIE(len < buf.size() - 3) << "too long parameter";
    std::strncpy(buf.get(), pat + 1, buf.size());
    buf[len-2] = '\0';
    scoped_fixed_array<char *, BUF_SIZE> col;
    const size_t n = tokenize(buf.get(), "|", col.get(), col.size());
    CHECK_DIE(n < col.size()) << "too long OR nodes";
    for (size_t i = 0; i < n; ++i) {
      patterns_.push_back(std::string(col[i]));
    }
  } else {
    patterns_.push_back(std::string(pat));
  }

  return !patterns_.empty();
}

bool PatternMatcher::match(const char *str) const {
  for (size_t i = 0; i < patterns_.size(); ++i) {
    if (patterns_[i] == str) {
      return matched_result_;
    }
  }
  return !matched_result_;
}

bool PatternMatcher::prefix_match(const char *str) const {
  const size_t len = std::strlen(str);
  for (size_t i = 0; i < patterns_.size(); ++i) {
    if (len < patterns_[i].size()) {
      continue;
    }
    if (0 == std::memcmp(str, patterns_[i].data(),
                         patterns_[i].size())) {
      return matched_result_;
    }
  }
  return !matched_result_;
}

Selector::Selector() {}
Selector::~Selector() {}

void Selector::close() {}

bool Selector::open(const Param &param) {
  Iconv iconv;
  iconv.open(UTF8, charset());
  CHECK_DIE(pat_ipa_func_.compile(IPA_FUNC_PAT, &iconv));
  CHECK_DIE(pat_ipa_head_.compile(IPA_HEAD_PAT, &iconv));
  CHECK_DIE(pat_juman_func_.compile(JUMAN_FUNC_PAT, &iconv));
  CHECK_DIE(pat_juman_head_.compile(JUMAN_HEAD_PAT, &iconv));
  CHECK_DIE(pat_unidic_func_.compile(UNIDIC_FUNC_PAT, &iconv));
  CHECK_DIE(pat_unidic_head_.compile(UNIDIC_HEAD_PAT, &iconv));
  CHECK_DIE(pat_unidic_func2_.compile(UNIDIC_FUNC_PAT2, &iconv));
  CHECK_DIE(pat_unidic_head2_.compile(UNIDIC_HEAD_PAT2, &iconv));
  CHECK_DIE(pat_unidic_head_pre_.compile(UNIDIC_HEAD_PRE_PAT, &iconv));
  CHECK_DIE(pat_kutouten_.compile(KUTOUTEN_PAT, &iconv));
  CHECK_DIE(pat_open_bracket_.compile(OPEN_BRACKET_PAT,  &iconv));
  CHECK_DIE(pat_close_bracket_.compile(CLOSE_BRACKET_PAT, &iconv));
  CHECK_DIE(pat_dyn_a_.compile(DYN_A_PAT, &iconv));
  CHECK_DIE(pat_case_.compile(CASE_PAT, &iconv));
  return true;
}

bool Selector::parse(Tree *tree) const {
  const size_t size = tree->chunk_size();
  const size_t pos_size = (tree->posset() == IPA) ? 4 : 2;

  for (size_t i = 0; i < size; ++i) {  // for all chunks
    const Chunk *chunk = tree->chunk(i);
    const size_t token_size = chunk->token_pos + chunk->token_size;
    const size_t kFeatureBufferSize = 2048;
    char *buf = tree->alloc(kFeatureBufferSize);
    std::ostrstream ostrs(buf, kFeatureBufferSize);

    // for all tokens
    for (size_t j = chunk->token_pos; j < token_size; ++j) {
      const Token *token = tree->token(j);
      if (pat_kutouten_.match(token->normalized_surface)) {
        ostrs << " GPUNC:" << token->normalized_surface;
        ostrs << " FPUNC:" << token->normalized_surface;
      }

      if (pat_open_bracket_.match(token->normalized_surface)) {
        ostrs << " GOB:" << token->normalized_surface;
        ostrs << " FOB:" << token->normalized_surface;
        ostrs << " GOB:1";
        ostrs << " FOB:1";
      }

      if (pat_close_bracket_.match(token->normalized_surface)) {
        ostrs << " GCB:" << token->normalized_surface;
        ostrs << " FCB:" << token->normalized_surface;
        ostrs << " GCB:1";
        ostrs << " FCB:1";
      }

      // all particles in a chunk
      if (pat_case_.prefix_match(token->feature)) {
        ostrs << " FCASE:" << token->normalized_surface;
      }
    }

    size_t head_index = 0;
    size_t func_index = 0;
    findHead(*tree, *chunk, &head_index, &func_index);

    const Token *htoken = tree->token(head_index);
    const Token *ftoken = tree->token(func_index);
    const Token *ltoken = tree->token(chunk->token_pos);
    const Token *rtoken = tree->token(chunk->token_pos +
                                      chunk->token_size - 1);

    // static features
    emitTokenFeatures("FH", htoken, pos_size, &ostrs);
    emitTokenFeatures("FF", ftoken, pos_size, &ostrs);
    emitTokenFeatures("FL", ltoken, pos_size, &ostrs);
    emitTokenFeatures("FR", rtoken, pos_size, &ostrs);

    // context features
    ostrs << " LF:" << ftoken->normalized_surface;
    ostrs << " RL:" << ltoken->normalized_surface;
    ostrs << " RH:" << htoken->normalized_surface;
    ostrs << " RF:" << ftoken->normalized_surface;

    if (i == 0) {
      ostrs << " FBOS:1";
    }
    if (i == size - 1) {
      ostrs << " FEOS:1";
    }

    if (pat_case_.prefix_match(ftoken->feature)) {
      ostrs << " GCASE:" << ftoken->normalized_surface;
    }

    // dynamic features
    const char *fcform = getToken(ftoken, pos_size + 1);
    if (pat_dyn_a_.prefix_match(ftoken->feature)) {
      ostrs << " A:" << ftoken->normalized_surface;
    } else if (fcform) {
      ostrs << " A:" << fcform;
    } else {
      std::string output;
      concat_feature(ftoken, pos_size, &output);
      ostrs << " A:" << output;
    }

    // This feature is not used for linear algorithm.
    //    std::string output;
    //    concat_feature(htoken, pos_size, &output);
    //    ostrs << " B:" << output;

    ostrs << std::ends;

    // write to tree
    Chunk *mutable_chunk = tree->mutable_chunk(i);
    mutable_chunk->head_pos = head_index - chunk->token_pos;
    mutable_chunk->func_pos = func_index - chunk->token_pos;

    const int kFeatureSize = 128;
    scoped_array<char *> feature(new char *[kFeatureSize]);
    const size_t s = tokenize(buf + 1, " ", feature.get(), kFeatureSize);
    mutable_chunk->feature_list_size = static_cast<unsigned char>(s);
    mutable_chunk->feature_list = const_cast<const char **>
        (tree->alloc_char_array(s));
    std::copy(feature.get(), feature.get() + s,
              mutable_chunk->feature_list);
  }

  tree->set_output_layer(OUTPUT_SELECTION);

  return true;
}

void Selector::findHead(const Tree &tree, const Chunk &chunk,
                        size_t *head_index, size_t *func_index) const {
  *head_index = chunk.token_pos;
  *func_index = chunk.token_pos;
  const size_t token_size = chunk.token_pos + chunk.token_size;

  const PatternMatcher *func_matcher = NULL;
  const PatternMatcher *head_matcher = NULL;

  switch (tree.posset()) {
    case UNIDIC:
      head_matcher = &pat_unidic_head2_;
      func_matcher = &pat_unidic_func2_;
      break;
    case IPA:
      head_matcher = &pat_ipa_head_;
      func_matcher = &pat_ipa_func_;
      break;
    case JUMAN:
      head_matcher = &pat_juman_head_;
      func_matcher = &pat_juman_func_;
      break;
    default:
      return;
  }

  for (size_t i = chunk.token_pos; i < token_size; ++i) {
    const Token *token = tree.token(i);
    if (func_matcher->prefix_match(token->feature)) {
      *func_index = i;
    }
    if (head_matcher->prefix_match(token->feature)) {
      *head_index = i;
    }
  }

  if ((tree.posset() == IPA || tree.posset() == UNIDIC) &&
      *head_index > *func_index) {
    *func_index = *head_index;
  }
}
}
