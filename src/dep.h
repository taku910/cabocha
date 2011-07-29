// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: dep.h 44 2008-02-03 14:59:21Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_DEP_H_
#define CABOCHA_DEP_H_

#include <vector>
#include <set>
#include "analyzer.h"
#include "scoped_ptr.h"

namespace CaboCha {

  class SVMBase;

  class cmpstr {
  public:bool operator() (const char *s1, const char *s2) {
    return (strcmp (s1, s2) < 0);}
  };

  class DependencyParser: public Analyzer {
  public:
    bool open(const Param &param);
    void close();
    bool parse(Tree *tree);
    explicit DependencyParser();
    virtual ~DependencyParser();

  private:
    scoped_ptr<SVMBase> svm_;
    std::vector<std::vector <char *> > static_feature_;
    std::vector<std::vector <char *> > dyn_b_feature_;
    std::vector<std::vector <char *> > dyn_b_;
    std::vector<std::vector <char *> > dyn_a_feature_;
    std::vector<std::vector <char *> > dyn_a_;
    std::vector<std::vector <char *> > gap_;
    std::vector<std::vector <char *> > gap_list_;
    const char *fp_[1024];
    std::set<const char *, cmpstr> fpset_;

    size_t build(Tree *tree);
    bool estimate(const Tree *tree,
                  int src, int dst,
                  double *score);
  };
}

#endif
