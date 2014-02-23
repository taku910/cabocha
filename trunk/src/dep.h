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
#include "freelist.h"
#include "analyzer.h"
#include "scoped_ptr.h"

namespace CaboCha {

class SVMModelInterface;

struct Hypothesis {
  void init(size_t size);
  std::vector<int> head;
  std::vector<float> score;
  std::vector<std::vector<int> > children;
  double hscore;
};

struct ChunkInfo {
  std::vector<char *> str_static_feature;
  std::vector<char *> str_gap_feature;
  std::vector<char *> str_left_context_feature;
  std::vector<char *> str_right_context_feature;
  std::vector<char *> str_child_feature;
  std::vector<int> src_static_feature;
  std::vector<int> dst1_static_feature;
  std::vector<int> dst2_static_feature;
  std::vector<int> left_context_feature;
  std::vector<int> right1_context_feature;
  std::vector<int> right2_context_feature;
  std::vector<int> src_child_feature;
  std::vector<int> dst1_child_feature;
  std::vector<int> dst2_child_feature;
//  std::vector<int> gap_feature;
  void clear();
};

struct DependencyParserData {
  std::vector<ChunkInfo> chunk_info;
  std::vector<int> fp;

  //  Agenda *agenda();
  void set_hypothesis(Hypothesis *hypothesis);
  Hypothesis *hypothesis();

  Hypothesis *hypothesis_;                   // current hypothesis
  scoped_ptr<Hypothesis> hypothesis_data_;   // used in greedy parsing

  DependencyParserData();
  ~DependencyParserData();
};

class DependencyParser: public Analyzer {
 public:
  bool open(const Param &param);
  void close();
  bool parse(Tree *tree) const;

  SVMModelInterface *mutable_svm_model() {
    return svm_.get();
  }

  explicit DependencyParser();
  virtual ~DependencyParser();

 private:
  bool parseShiftReduce(Tree *tree) const;
  void build(Tree *tree) const;
  bool estimate(const Tree *tree,
                int src, int dst,
                double *score) const;

  scoped_ptr<SVMModelInterface> svm_;
};
}
#endif
