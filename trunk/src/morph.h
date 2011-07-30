// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: morph.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_MORPH_H_
#define CABOCHA_MORPH_H_

#include "analyzer.h"
#include <string>

struct mecab_t;

namespace CaboCha {

class MorphAnalyzer: public Analyzer {
 public:
  bool open(const Param &) ;
  void close();
  bool parse(Tree *);

  explicit MorphAnalyzer();
  virtual ~MorphAnalyzer();

 private:
  mecab_t *mecab_;
  std::string sentence_;
};
}

#endif
