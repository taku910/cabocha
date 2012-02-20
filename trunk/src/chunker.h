// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: chunker.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_CHUNKER_H_
#define CABOCHA_CHUNKER_H_
#include "analyzer.h"

struct crfpp_t;

namespace CaboCha {

class Chunker: public Analyzer {
 public:
  bool open(const Param &param);
  void close();
  bool parse(Tree *tree) const;

  explicit Chunker();
  virtual ~Chunker();

 private:
  crfpp_model_t *model_;
};
}
#endif
