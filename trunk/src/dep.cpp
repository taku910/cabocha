// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: dep.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <algorithm>
#include <functional>
#include <fstream>
#include <iterator>
#include <stack>
#include <vector>
#include "cabocha.h"
#include "common.h"
#include "dep.h"
#include "param.h"
#include "svm.h"
#include "timer.h"
#include "tree_allocator.h"
#include "utils.h"

namespace CaboCha {

class HypothesisComp {
 public:
  bool operator() (const Hypothesis *a, const Hypothesis *b) const {
    return a->hscore < b->hscore;
  }
};

void Hypothesis::init(size_t size) {
  head.resize(size);
  score.resize(size);
  children.resize(size);
  hscore = 0.0;
  for (size_t i = 0; i < size; ++i) {
    head[i] = -1;
    score[i] = 0.0;
    children[i].clear();
  }
}

void DependencyParserData::set_hypothesis(Hypothesis *hypothesis) {
  hypothesis_ = hypothesis;
}

Hypothesis *DependencyParserData::hypothesis() {
  if (hypothesis_) {
    return hypothesis_;
  }
  if (!hypothesis_data_.get()) {
    hypothesis_data_.reset(new Hypothesis);
    hypothesis_ = hypothesis_data_.get();
  }
  return hypothesis_;
}

DependencyParserData::DependencyParserData() : hypothesis_(0) {}
DependencyParserData::~DependencyParserData() {}

DependencyParser::DependencyParser() : svm_(0) {}

DependencyParser::~DependencyParser() {}

bool DependencyParser::open(const Param &param) {
  close();

  if (action_mode() == PARSING_MODE) {
    const std::string filename = param.get<std::string>("parser-model");
    bool failed = false;
    svm_.reset(new FastSVMModel);
    if (!svm_->open(filename.c_str())) {
      WHAT << svm_->what() << "\n";
      svm_.reset(new ImmutableSVMModel);
      if (!svm_->open(filename.c_str())) {
        WHAT << svm_->what();
        failed = true;
      }
    }
    CHECK_FALSE(!failed) << "no such file or directory: "
                         << filename;

    const char *c = svm_->get_param("charset");
    CHECK_FALSE(c) << "charset is not defined";
    CHECK_FALSE(decode_charset(c) == charset())
        << "model charset and dependency parser's charset are different: "
        << c << " != " << encode_charset(charset());

    const char *p = svm_->get_param("posset");
    CHECK_FALSE(p) << "posset is not defined";
    CHECK_FALSE(decode_posset(p) == posset())
        << "model posset and dependency parser's posset are different: "
        << p << " != " << encode_posset(posset());
  }

  if (action_mode() == TRAINING_MODE) {
    svm_.reset(new SVMModel);
  }

  return true;
}

void DependencyParser::close() {
  svm_.reset(0);
}

void ChunkInfo::clear() {
  str_static_feature.clear();
  str_gap_feature.clear();
  str_left_context_feature.clear();
  str_right_context_feature.clear();
  str_child_feature.clear();
  src_static_feature.clear();
  dst1_static_feature.clear();
  dst2_static_feature.clear();
  left_context_feature.clear();
  right1_context_feature.clear();
  right2_context_feature.clear();
  src_child_feature.clear();
  dst1_child_feature.clear();
  dst2_child_feature.clear();
}

#define ADD_FEATURE(key) do { \
  const int id = svm_->id(key); \
  if (id != -1) { fp->push_back(id); }          \
  } while (0)

#define ADD_FEATURE2(key, array) do {           \
  const int id = svm_->id(key);                 \
    if (id != -1) { (array).push_back(id); }    \
  } while (0)

#define COPY_FEATURE(src_array) do { \
    std::copy((src_array).begin(), (src_array).end(), \
              std::back_inserter(*fp));               \
  } while (0)

void DependencyParser::build(Tree *tree) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  data->chunk_info.resize(tree->chunk_size());
  for (size_t i = 0; i < data->chunk_info.size(); ++i) {
    data->chunk_info[i].clear();
  }

  // collect all features from each chunk.
  for (size_t i = 0; i < tree->chunk_size(); ++i) {
    Chunk *chunk = tree->mutable_chunk(i);
    ChunkInfo *chunk_info = &data->chunk_info[i];
    CHECK_DIE(chunk_info);
    for (size_t k = 0; k < chunk->feature_list_size; ++k) {
      char *feature = const_cast<char *>(chunk->feature_list[k]);
      CHECK_DIE(feature);
      switch (feature[0]) {
        case 'F':
          chunk_info->str_static_feature.push_back(feature);
          break;
        case 'L':
          chunk_info->str_left_context_feature.push_back(feature);
          break;
        case 'R':
          chunk_info->str_right_context_feature.push_back(feature);
          break;
        case 'G':
          chunk_info->str_gap_feature.push_back(feature);
          break;
        case 'A':
          chunk_info->str_child_feature.push_back(feature);
          break;
        default:
          CHECK_DIE(true) << "Unknown feature " << feature;
      }
    }
  }
}

bool DependencyParser::estimate(const Tree *tree, int src, int dst,
                                double *score) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  Hypothesis *hypo = data->hypothesis();
  CHECK_DIE(hypo);

  std::vector<int> *fp = &data->fp;
  fp->clear();

  // distance features
  const int dist = dst - src;
  if (dist == 1) {
    ADD_FEATURE("DIST:1");
  } else if (dist >= 2 && dist <= 5) {
    ADD_FEATURE("DIST:2-5");
  } else {
    ADD_FEATURE("DIST:6-");
  }

  {
    ChunkInfo *chunk_info = &data->chunk_info[src];
    if (chunk_info->src_static_feature.empty()) {
      for (size_t i = 0; i < chunk_info->str_static_feature.size(); ++i) {
        chunk_info->str_static_feature[i][0] = 'S';
        ADD_FEATURE2(chunk_info->str_static_feature[i],
                     chunk_info->src_static_feature);
      }
    }
    COPY_FEATURE(chunk_info->src_static_feature);
  }

  {
    ChunkInfo *chunk_info = &data->chunk_info[dst];
    if (chunk_info->dst1_static_feature.empty()) {
      for (size_t i = 0; i < chunk_info->str_static_feature.size(); ++i) {
        chunk_info->str_static_feature[i][0] = 'D';
        ADD_FEATURE2(chunk_info->str_static_feature[i],
                     chunk_info->dst1_static_feature);
      }
    }
    COPY_FEATURE(chunk_info->dst1_static_feature);
  }

  if (src > 0) {
    ChunkInfo *chunk_info = &data->chunk_info[src - 1];
    if (chunk_info->left_context_feature.empty()) {
      for (size_t i = 0;
           i < chunk_info->str_left_context_feature.size(); ++i) {
        ADD_FEATURE2(chunk_info->str_left_context_feature[i],
                     chunk_info->left_context_feature);
      }
    }
    COPY_FEATURE(chunk_info->left_context_feature);
  }

  if (dst < static_cast<int>(tree->chunk_size() - 1)) {
    ChunkInfo *chunk_info = &data->chunk_info[dst + 1];
    if (chunk_info->right1_context_feature.empty()) {
      for (size_t i = 0;
           i < chunk_info->str_right_context_feature.size(); ++i) {
        ADD_FEATURE2(chunk_info->str_right_context_feature[i],
                     chunk_info->right1_context_feature);
      }
    }
    COPY_FEATURE(chunk_info->right1_context_feature);
  }

  for (size_t i = 0; i < hypo->children[src].size(); ++i) {
    const int child = hypo->children[src][i];
    ChunkInfo *chunk_info = &data->chunk_info[child];
    if (chunk_info->src_child_feature.empty()) {
      for (size_t j = 0; j < chunk_info->str_child_feature.size(); ++j) {
        chunk_info->str_child_feature[j][0] = 'a';
        ADD_FEATURE2(chunk_info->str_child_feature[j],
                     chunk_info->src_child_feature);
      }
    }
    COPY_FEATURE(chunk_info->src_child_feature);
  }

  for (size_t i = 0; i < hypo->children[dst].size(); ++i) {
    const int child = hypo->children[dst][i];
    ChunkInfo *chunk_info = &data->chunk_info[child];
    if (chunk_info->dst1_child_feature.empty()) {
      for (size_t j = 0; j < chunk_info->str_child_feature.size(); ++j) {
        chunk_info->str_child_feature[j][0] = 'A';
        ADD_FEATURE2(chunk_info->str_child_feature[j],
                     chunk_info->dst1_child_feature);
      }
    }
    COPY_FEATURE(chunk_info->dst1_child_feature);
  }

  // gap features
  int bracket_status = 0;
  for (int k = src + 1; k <= dst - 1; ++k) {
    ChunkInfo *chunk_info = &data->chunk_info[k];
    for (size_t i = 0; i < chunk_info->str_gap_feature.size(); ++i) {
      const char *gap_feature = chunk_info->str_gap_feature[i];
      if (std::strcmp(gap_feature, "GOB:1") == 0) {
        bracket_status |= 1;
      } else if (std::strcmp(gap_feature, "GCB:1") == 0) {
        bracket_status |= 2;
      } else {
        ADD_FEATURE(gap_feature);
      }
    }
  }

  // bracket status
  switch (bracket_status) {
    case 0: ADD_FEATURE("GNB:1"); break;  // nothing
    case 1: ADD_FEATURE("GOB:1"); break;  // open only
    case 2: ADD_FEATURE("GCB:1"); break;  // close only
    default: ADD_FEATURE("GBB:1"); break;  // both
  }

  std::sort(fp->begin(), fp->end());
  fp->erase(std::unique(fp->begin(), fp->end()), fp->end());

  if (action_mode() == PARSING_MODE) {
    *score = svm_->classify(*fp);
    return *score > 0;
  } else {
    CHECK_DIE(!fp->empty());
    const bool isdep = (tree->chunk(src)->link == dst);
    svm_->add(isdep ? +1 : -1, *fp);
    return isdep;
  }

  return false;
}

#define MYPOP(agenda, n) do {                   \
    if (agenda.empty()) {                       \
      n = -1;                                   \
    } else {                                    \
      n = agenda.top();                         \
      agenda.pop();                             \
    }                                           \
  } while (0)

// Sassano's algorithm
bool DependencyParser::parseShiftReduce(Tree *tree) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  const int size = static_cast<int>(tree->chunk_size());
  CHECK_DIE(size >= 2);

  Hypothesis *hypo = data->hypothesis();
  CHECK_DIE(hypo);
  hypo->init(size);

  std::stack<int> agenda;
  double score = 0.0;
  agenda.push(0);

  for (int dst = 1; dst < size; ++dst) {
    int src = 0;
    MYPOP(agenda, src);

    // |is_fake_link| is used for partial training, where
    // not all dependency relations are specified in the training phase.
    // Here we assume that a chunk modifes the next chunk,
    // if the dependency relation is unknown. We don't use the fake
    // dependency for training.
    const bool is_fake_link =
        (action_mode() == TRAINING_MODE &&
         dst != size - 1 &&
         tree->chunk(src)->link == -1);

    // if agenda is empty, src == -1.
    while (src != -1 &&
           (dst == size - 1 || is_fake_link ||
            estimate(tree, src, dst, &score))) {
      hypo->head[src] = dst;
      hypo->score[src] = score;
      // store children for dynamic_features
      if (!is_fake_link) {
        hypo->children[dst].push_back(src);
      }

      MYPOP(agenda, src);
    }
    if (src != -1) {
      agenda.push(src);
    }
    agenda.push(dst);
  }

  for (int src = 0; src < size - 1; ++src) {
    Chunk *chunk = tree->mutable_chunk(src);
    chunk->link = hypo->head[src];
    chunk->score = hypo->score[src];
  }

  return true;
}
#undef MYPOP

bool DependencyParser::parse(Tree *tree) const {
  if (!tree->allocator()->dependency_parser_data) {
    tree->allocator()->dependency_parser_data
        = new DependencyParserData;
    CHECK_DIE(tree->allocator()->dependency_parser_data);
  }

  tree->set_output_layer(OUTPUT_DEP);

  if (tree->chunk_size() == 0) {
    return true;
  }

  if (tree->chunk_size() == 1) {
    tree->mutable_chunk(tree->chunk_size() - 1)->link = -1;
    tree->mutable_chunk(tree->chunk_size() - 1)->score = 0.0;
    return true;
  }

  CHECK_DIE(svm_.get());

  // make features
  build(tree);

  return parseShiftReduce(tree);
}
}  // namespace CaboCha
