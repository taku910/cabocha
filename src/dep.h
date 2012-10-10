// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: dep.h 44 2008-02-03 14:59:21Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_DEP_H_
#define CABOCHA_DEP_H_

#include <vector>
#include <set>
#include "common.h"
#include "analyzer.h"
#include "scoped_ptr.h"

namespace CaboCha {

class SVMInterface;

struct DependencyParserData {
  struct Result {
    double score;
    int link;
  };
  std::vector<std::vector <char *> > static_feature;
  std::vector<std::vector <char *> > left_context_feature;
  std::vector<std::vector <char *> > right_context_feature;
  std::vector<std::vector <char *> > gap_feature;
  std::vector<std::vector <char *> > dyn_b_feature;
  std::vector<std::vector <char *> > dyn_b;
  std::vector<std::vector <char *> > dyn_a_feature;
  std::vector<std::vector <char *> > dyn_a;
  std::vector<Result> results;
  hash_set<const char *> fpset;
  std::vector<const char *> fp;
};

class DependencyParser: public Analyzer {
 public:
  bool open(const Param &param);
  void close();
  bool parse(Tree *tree) const;
  explicit DependencyParser();
  virtual ~DependencyParser();

 private:
  void build(Tree *tree) const;
  bool estimate(const Tree *tree,
                int src, int dst,
                double *score) const;
  scoped_ptr<SVMInterface> svm_;
};
}
#endif
