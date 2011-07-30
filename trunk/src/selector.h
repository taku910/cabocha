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
  explicit PatternMatcher() {}
  virtual ~PatternMatcher() { clear(); }

  bool compile(const char *pat, Iconv *iconv);

  void clear() {
    patterns_.clear();
  }

  const char* match(const char *str) const {
    for (size_t i = 0; i < patterns_.size(); ++i) {
      if (patterns_[i] == str) {
        return patterns_[i].c_str();
      }
    }
    return 0;
  }

  const char* prefix_match(const char *str) const {
    const size_t len = strlen(str);
    for (size_t i = 0; i < patterns_.size(); ++i) {
      if (len > patterns_[i].size()) {
        continue;
      }
      if (0 == memcmp(str, patterns_[i].data(),
                      patterns_[i].size())) {
        return patterns_[i].c_str();
      }
    }
    return 0;
  }

 private:
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
  PatternMatcher pat_kutouten_, pat_open_bracket_, pat_close_bracket_;
  PatternMatcher pat_dyn_a_, pat_case_;
  PatternMatcher pat_ipa_func_, pat_ipa_head_;
  PatternMatcher pat_juman_func_, pat_juman_head_;
};
}
#endif
