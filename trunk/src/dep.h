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
class SVMModel;

class cmpstr {
 public:bool operator() (const char *s1, const char *s2) {
   return (strcmp(s1, s2) < 0); }
};

struct Hypothesis {
  void init(size_t size);
  std::vector<int> head;
  std::vector<float> score;
  std::vector<std::vector<int> > children;
  double hscore;
};

// // stack-based agenda.
// struct Agenda {
//  public:
//   Agenda();
//   ~Agenda();
//   void init(size_t size);
//   Hypothesis *alloc();
//   void push(Hypothesis *hypo, size_t index);
//   Hypothesis *pop(size_t index);

//  private:
//   FreeList<Hypothesis> freelist_;
//   std::vector<std::vector<Hypothesis *> > agenda_;
// };

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
  std::vector<int> gap_feature;

  void clear();
};

struct DependencyParserData {
  std::vector<ChunkInfo> chunk_info;
  std::vector<int> fp;

  //  Agenda *agenda();
  void set_hypothesis(Hypothesis *hypothesis);
  Hypothesis *hypothesis();

  Hypothesis *hypothesis_;                   // current hypothesis
  scoped_ptr<Hypothesis> hypothesis_data_;  // used in greedy parsing

  DependencyParserData();
  ~DependencyParserData();
};

class DependencyParser: public Analyzer {
 public:
  bool open(const Param &param);
  void close();
  bool parse(Tree *tree) const;
  void set_parsing_algorithm(int parsing_algorithm) {
    parsing_algorithm_ = parsing_algorithm;
  }
  int parsing_algorithm() const {
    return parsing_algorithm_;
  }

  SVMModelInterface *mutable_svm_model() {
    return svm_.get();
  }

  explicit DependencyParser();
  virtual ~DependencyParser();

 private:
  bool parseShiftReduce(Tree *tree) const;
  bool parseTournament(Tree *tree) const;
  void build(Tree *tree) const;
  bool estimate(const Tree *tree,
                int src, int dst,
                double *score) const;
  bool estimate(const Tree *tree,
                int src, int dst1, int dst2,
                double *score) const;

  scoped_ptr<SVMModelInterface> svm_;
  int parsing_algorithm_;
  //  int beam_;
};
}
#endif
