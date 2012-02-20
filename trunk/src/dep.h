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

class SVMInterface;

class cmpstr {
 public:bool operator() (const char *s1, const char *s2) {
   return (strcmp(s1, s2) < 0);}
};

struct DependencyParserData {
  struct Result {
    double score;
    int link;
  };
  std::vector<std::vector <char *> > static_feature;
  std::vector<std::vector <char *> > dyn_b_feature;
  std::vector<std::vector <char *> > dyn_b;
  std::vector<std::vector <char *> > dyn_a_feature;
  std::vector<std::vector <char *> > dyn_a;
  std::vector<std::vector <char *> > gap;
  std::vector<std::vector <char *> > gap_list;
  std::vector<Result> results;
  const char *fp[1024];
  std::set<const char *, cmpstr> fpset;
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
