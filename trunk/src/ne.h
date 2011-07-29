// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: ne.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_NE_H_
#define CABOCHA_NE_H_

#include <vector>
#include <string>
#include "cabocha.h"
#include "analyzer.h"

struct crfpp_t;

namespace CaboCha {

  class NE: public Analyzer {
  public:
    bool open(const Param &param);
    bool parse(Tree *tree);
    void close();

    explicit NE();
    virtual ~NE();

  private:
    crfpp_t *tagger_;
    std::vector<const char*> feature_;
    std::string ne_composite_;
  };
}
#endif
