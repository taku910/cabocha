// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: analyzer.h 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_ANALYZER_H_
#define CABOCHA_ANALYZER_H_

#include <iostream>
#include <string>
#include <vector>
#include "cabocha.h"
#include "common.h"
#include "ucs.h"

namespace CaboCha {

class Param;

struct FeatureEventManager {
  std::vector<float> real_feature;
  std::vector<std::string> general_feature;
};

class Analyzer {
 public:
  const char *what() { return what_.str(); }

  virtual bool open(const Param &) = 0;
  virtual void close() = 0;
  virtual bool parse(Tree *tree) const = 0;

  int action_mode() const { return action_mode_; }
  void set_action_mode(int action_mode) {
    action_mode_ = action_mode;
  }

  CharsetType charset() const { return charset_; }
  void set_charset(CharsetType charset) { charset_ = charset; }

  PossetType posset() const { return posset_; }
  void set_posset(PossetType posset) { posset_ = posset; }

  void setFeatureExtractor(
      const FeatureExtractorInterface *feature_extractor) {
    feature_extractor_ = feature_extractor;
  }

  const FeatureExtractorInterface *feature_extractor() const {
    return feature_extractor_;
  }

  explicit Analyzer() : action_mode_(PARSING_MODE),
                        charset_(EUC_JP),
                        posset_(IPA),
                        feature_extractor_(0) {}
  virtual ~Analyzer() {}

 protected:
  whatlog what_;

 private:
  int action_mode_;
  CharsetType charset_;
  PossetType posset_;
  const FeatureExtractorInterface *feature_extractor_;
};
}
#endif
