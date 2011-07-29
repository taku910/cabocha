// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: analyzer.h 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_ANALYZER_H_
#define CABOCHA_ANALYZER_H_

#include <iostream>
#include "cabocha.h"
#include "common.h"
#include "ucs.h"

namespace CaboCha {

class Param;

class Analyzer {
 private:
  int action_mode_;
  std::ostream *os_;
  CharsetType charset_;
  PossetType posset_;

 protected:
  whatlog what_;

 public:
  const char *what() { return what_.str(); }

  virtual bool open(const Param &) = 0;
  virtual void close() = 0;
  virtual bool parse(Tree *tree) = 0;

  int action_mode() const { return action_mode_; }
  void set_action_mode(int action_mode) {
    action_mode_ = action_mode;
  }

  std::ostream *stream() const { return os_; }
  void set_stream(std::ostream *os) {
    os_ = os;
  }

  CharsetType charset() const { return charset_; }
  void set_charset(CharsetType charset) { charset_ = charset; }

  PossetType posset() const { return posset_; }
  void set_posset(PossetType posset) { posset_ = posset; }

  explicit Analyzer(): action_mode_(PARSING_MODE),
                       os_(&std::cout),
                       charset_(EUC_JP),
                       posset_(IPA) {}
  virtual ~Analyzer() {}
};
}

#endif
