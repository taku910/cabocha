// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: selector.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_SELECTOR_H_
#define CABOCHA_SELECTOR_H_

#include <cstring>
#include <string>
#include <vector>
#include "analyzer.h"
#include "cabocha.h"
#include "common.h"

namespace CaboCha {

class Iconv;

class PatternMatcher {
 public:
  PatternMatcher();
  virtual ~PatternMatcher();

  bool compile(const char *pat, Iconv *iconv);
  void clear();
  bool match(const char *str) const;
  bool prefix_match(const char *str) const;

 private:
  bool matched_result_;
  std::vector<std::string> patterns_;
};

class Selector: public Analyzer {
 public:
  bool open(const Param &param);
  void close();
  bool parse(Tree *tree);
  explicit Selector();
  virtual ~Selector();

 private:
  void findHead(const Tree &tree, const Chunk &chunk, size_t *hid, size_t *fid) const;

  PatternMatcher pat_kutouten_, pat_open_bracket_, pat_close_bracket_;
  PatternMatcher pat_dyn_a_, pat_case_;
  PatternMatcher pat_ipa_func_, pat_ipa_head_;
  PatternMatcher pat_juman_func_, pat_juman_head_;
  PatternMatcher pat_unidic_func_, pat_unidic_head_, pat_unidic_func2_, pat_unidic_head2_, pat_unidic_head_pre_;
};
}
#endif
