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

// Agenda::Agenda() : freelist_(256) {}
// Agenda::~Agenda() {}

// void Agenda::init(size_t size) {
//   agenda_.resize(size);
//   for (size_t i = 0; i < size; ++i) {
//     agenda_[i].clear();
//   }
//   freelist_.free();
// }

// Hypothesis *Agenda::alloc() {
//   return freelist_.alloc();
// }

// void Agenda::push(Hypothesis *hypo, size_t index) {
//   agenda_[index].push_back(hypo);
// }

// Hypothesis *Agenda::pop(size_t index) {
//   if (agenda_[index].empty()) {
//     return 0;
//   }
//   std::pop_heap(agenda_[index].begin(),
//                 agenda_[index].end(), HypothesisComp());
//   Hypothesis *result = agenda_[index].back();
//   agenda_[index].resize(agenda_[index].size() - 1);
//   return result;
// }

// Agenda *DependencyParserData::agenda() {
//   if (!agenda_.get()) {
//     agenda_.reset(new Agenda);
//   }
//   return agenda_.get();
// }

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

DependencyParser::DependencyParser()
    : svm_(0), parsing_algorithm_(SHIFT_REDUCE), beam_(1) {}

DependencyParser::~DependencyParser() {}

bool DependencyParser::open(const Param &param) {
  close();

  if (action_mode() == PARSING_MODE) {
    const std::string filename = param.get<std::string>("parser-model");
    bool failed = false;
    svm_.reset(new FastSVMModel);
    if (!svm_->open(filename.c_str())) {
      WHAT << svm_->what() << "\n";
      svm_.reset(new SVMModel);
      if (!svm_->open(filename.c_str())) {
        WHAT << svm_->what();
        failed = true;
      }
    }
    CHECK_FALSE(!failed) << "no such file or directory: "
                         << filename;

    const char *v = svm_->get_param("parsing_algorithm");
    CHECK_FALSE(v) << "parsing_algorithm is not defined";
    parsing_algorithm_ = std::atoi(v);
    CHECK_FALSE(parsing_algorithm_ == CABOCHA_TOURNAMENT ||
                parsing_algorithm_ == CABOCHA_SHIFT_REDUCE);

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

    //    beam_ = param.get<int>("beam");
    //    CHECK_FALSE(beam_ >= 1 && beam_ <= 100)
    //        << "beam width must be 1<=beam<=100";
  }

  return true;
}

void DependencyParser::close() {
  svm_.reset(0);
}

#define INIT_FEATURE(array) do {                \
    array.clear();                              \
    array.resize(size);                         \
  } while (false)

void DependencyParser::build(Tree *tree) const {
  const size_t size  = tree->chunk_size();

  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  INIT_FEATURE(data->static_feature);
  INIT_FEATURE(data->left_context_feature);
  INIT_FEATURE(data->right_context_feature);
  INIT_FEATURE(data->gap_feature);
  INIT_FEATURE(data->dynamic_feature);

  // collect all features from each chunk.
  for (size_t i = 0; i < tree->chunk_size(); ++i) {
    Chunk *chunk = tree->mutable_chunk(i);
    for (size_t k = 0; k < chunk->feature_list_size; ++k) {
      char *feature = const_cast<char *>(chunk->feature_list[k]);
      switch (feature[0]) {
        case 'F': data->static_feature[i].push_back(feature); break;
        case 'L': data->left_context_feature[i].push_back(feature); break;
        case 'R': data->right_context_feature[i].push_back(feature); break;
        case 'G': data->gap_feature[i].push_back(feature);    break;
        case 'A': data->dynamic_feature[i].push_back(feature);  break;
        default:
          CHECK_DIE(true) << "Unknown feature " << feature;
      }
    }
  }
}

bool DependencyParser::estimate(const Tree *tree,
                                int src, int dst1, int dst2,
                                double *score) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  Hypothesis *hypo = data->hypothesis();
  CHECK_DIE(hypo);

  std::set<std::string> fpset;
  CHECK_DIE(dst1 != dst2);

  // distance features
  const int dist1 = dst1 - src;
  const int dist2 = dst2 - src;
  if (dist1 == 1) {
    fpset.insert("DIST1:1");
  } else if (dist1 >= 2 && dist1 <= 5) {
    fpset.insert("DIST1:2-5");
  } else {
    fpset.insert("DIST1:6-");
  }

  if (dist2 == 1) {
    fpset.insert("DIST2:1");
  } else if (dist2 >= 2 && dist2 <= 5) {
    fpset.insert("DIST2:2-5");
  } else {
    fpset.insert("DIST2:6-");
  }

  // static features
  for (size_t i = 0; i < data->static_feature[src].size(); ++i) {
    data->static_feature[src][i][0] = 'S';  // src
    fpset.insert(data->static_feature[src][i]);
  }

  for (size_t i = 0; i < data->static_feature[dst1].size(); ++i) {
    std::string n = "D1";
    n += data->static_feature[dst1][i];
    fpset.insert(n);
  }

  for (size_t i = 0; i < data->static_feature[dst2].size(); ++i) {
    std::string n = "D2";
    n += data->static_feature[dst2][i];
    fpset.insert(n);
  }

  // left context features
  if (src > 0) {
    for (size_t i = 0; i < data->left_context_feature[src - 1].size(); ++i) {
      fpset.insert(data->left_context_feature[src - 1][i]);
    }
  }

  // right context features
  if (dst1 < static_cast<int>(tree->chunk_size() - 1)) {
    for (size_t i = 0; i < data->right_context_feature[dst1 + 1].size(); ++i) {
      std::string n = "R1";
      n += data->right_context_feature[dst1 + 1][i];
      fpset.insert(n);
    }
  }

  // right context features
  if (dst2 < static_cast<int>(tree->chunk_size() - 1)) {
    for (size_t i = 0; i < data->right_context_feature[dst2 + 1].size(); ++i) {
      std::string n = "R2";
      n += data->right_context_feature[dst2 + 1][i];
      fpset.insert(n);
    }
  }

  {
    // gap features
    int bracket_status = 0;
    for (int k = src + 1; k <= dst1 - 1; ++k) {
      for (size_t i = 0; i < data->gap_feature[k].size(); ++i) {
        const char *gap_feature = data->gap_feature[k][i];
        if (std::strcmp(gap_feature, "GOB:1") == 0) {
          bracket_status |= 1;
        } else if (std::strcmp(gap_feature, "GCB:1") == 0) {
          bracket_status |= 2;
        } else {
          std::string n = "G1";
          n += gap_feature;
          fpset.insert(n);
        }
      }
    }

    // bracket status
    switch (bracket_status) {
      case 0: fpset.insert("GNB1:1"); break;  // nothing
      case 1: fpset.insert("GOB1:1"); break;  // open only
      case 2: fpset.insert("GCB1:1"); break;  // close only
      default: fpset.insert("GBB1:1"); break;  // both
    }
  }

  {
    // gap features
    int bracket_status = 0;
    for (int k = src + 1; k <= dst2 - 1; ++k) {
      for (size_t i = 0; i < data->gap_feature[k].size(); ++i) {
        const char *gap_feature = data->gap_feature[k][i];
        if (std::strcmp(gap_feature, "GOB:1") == 0) {
          bracket_status |= 1;
        } else if (std::strcmp(gap_feature, "GCB:1") == 0) {
          bracket_status |= 2;
        } else {
          std::string n = "G2";
          n += gap_feature;
          fpset.insert(n);
        }
      }
    }

    // bracket status
    switch (bracket_status) {
      case 0: fpset.insert("GNB2:1"); break;  // nothing
      case 1: fpset.insert("GOB2:1"); break;  // open only
      case 2: fpset.insert("GCB2:1"); break;  // close only
      default: fpset.insert("GBB2:1"); break;  // both
    }
  }

  for (size_t i = 0; i < hypo->children[src].size(); ++i) {
    const int child = hypo->children[src][i];
    for (size_t j = 0; j < data->dynamic_feature[child].size(); ++j) {
      data->dynamic_feature[child][j][0] = 'a';
      fpset.insert(data->dynamic_feature[child][j]);
    }
  }

  {
    // dynamic features
    for (size_t i = 0; i < hypo->children[dst1].size(); ++i) {
      const int child = hypo->children[dst1][i];
      for (size_t j = 0; j < data->dynamic_feature[child].size(); ++j) {
        std::string n = "A1";
        n += data->dynamic_feature[child][j];
        fpset.insert(n);
      }
    }

    // dynamic features
    for (size_t i = 0; i < hypo->children[dst2].size(); ++i) {
      const int child = hypo->children[dst2][i];
      for (size_t j = 0; j < data->dynamic_feature[child].size(); ++j) {
        std::string n = "A2";
        n += data->dynamic_feature[child][j];
        fpset.insert(n);
      }
    }
  }

  std::vector<const char *> fp;
  for (std::set<std::string>::const_iterator it = fpset.begin();
       it != fpset.end(); ++it) {
    fp.push_back(it->c_str());
  }

  TreeAllocator *allocator = tree->allocator();

  if (action_mode() == PARSING_MODE) {
    *score = svm_->classify(fp);
    return *score > 0;
  } else {
    const int h = tree->chunk(src)->link;
    CHECK_DIE(h != -1 && h >= 0 && h < static_cast<int>(tree->size()));
    if (h == dst2) {  // RIGHT
      *(allocator->stream()) << "+1";
    } else if (h == dst1) {  // LEFT
      *(allocator->stream()) << "-1";
    } else {
      CHECK_DIE(true) << "dst1 or dst2 must be a true head";
    }
    for (size_t i = 0; i < fp.size(); ++i) {
      *(allocator->stream()) << ' ' << fp[i];
    }
    *(allocator->stream()) << std::endl;
  }

  return false;
}

bool DependencyParser::estimate(const Tree *tree, int src, int dst,
                                double *score) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  Hypothesis *hypo = data->hypothesis();
  CHECK_DIE(hypo);

  data->fpset.clear();

  // distance features
  const int dist = dst - src;
  if (dist == 1) {
    data->fpset.insert("DIST:1");
  } else if (dist >= 2 && dist <= 5) {
    data->fpset.insert("DIST:2-5");
  } else {
    data->fpset.insert("DIST:6-");
  }

  // static features
  for (size_t i = 0; i < data->static_feature[src].size(); ++i) {
    data->static_feature[src][i][0] = 'S';  // src
    data->fpset.insert(data->static_feature[src][i]);
  }

  for (size_t i = 0; i < data->static_feature[dst].size(); ++i) {
    data->static_feature[dst][i][0] = 'D';  // dst
    data->fpset.insert(data->static_feature[dst][i]);
  }

  // left context features
  if (src > 0) {
    for (size_t i = 0; i < data->left_context_feature[src - 1].size(); ++i) {
      data->fpset.insert(data->left_context_feature[src - 1][i]);
    }
  }

  // right context features
  if (dst < static_cast<int>(tree->chunk_size() - 1)) {
    for (size_t i = 0; i < data->right_context_feature[dst + 1].size(); ++i) {
      data->fpset.insert(data->right_context_feature[dst + 1][i]);
    }
  }

  // gap features
  int bracket_status = 0;
  for (int k = src + 1; k <= dst - 1; ++k) {
    for (size_t i = 0; i < data->gap_feature[k].size(); ++i) {
      const char *gap_feature = data->gap_feature[k][i];
      if (std::strcmp(gap_feature, "GOB:1") == 0) {
        bracket_status |= 1;
      } else if (std::strcmp(gap_feature, "GCB:1") == 0) {
        bracket_status |= 2;
      } else {
        data->fpset.insert(gap_feature);
      }
    }
  }

  // bracket status
  switch (bracket_status) {
    case 0: data->fpset.insert("GNB:1"); break;  // nothing
    case 1: data->fpset.insert("GOB:1"); break;  // open only
    case 2: data->fpset.insert("GCB:1"); break;  // close only
    default: data->fpset.insert("GBB:1"); break;  // both
  }

  // dynamic features
  for (size_t i = 0; i < hypo->children[dst].size(); ++i) {
    const int child = hypo->children[dst][i];
    for (size_t j = 0; j < data->dynamic_feature[child].size(); ++j) {
      data->dynamic_feature[child][j][0] = 'A';
      data->fpset.insert(data->dynamic_feature[child][j]);
    }
  }

  for (size_t i = 0; i < hypo->children[src].size(); ++i) {
    const int child = hypo->children[src][i];
    for (size_t j = 0; j < data->dynamic_feature[child].size(); ++j) {
      data->dynamic_feature[child][j][0] = 'a';
      data->fpset.insert(data->dynamic_feature[child][j]);
    }
  }

  data->fp.clear();
  std::copy(data->fpset.begin(), data->fpset.end(),
            std::back_inserter(data->fp));

  TreeAllocator *allocator = tree->allocator();

  if (action_mode() == PARSING_MODE) {
    *score = svm_->classify(data->fp);
    return *score > 0;
  } else {
    std::sort(data->fp.begin(), data->fp.end(), cmpstr());
    const bool isdep = (tree->chunk(src)->link == dst);
    *(allocator->stream()) << (isdep ? "+1" : "-1");
    for (size_t i = 0; i < data->fp.size(); ++i) {
      *(allocator->stream()) << ' ' << data->fp[i];
    }
    *(allocator->stream()) << std::endl;
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

  tree->set_output_layer(OUTPUT_DEP);
  return true;
}
#undef MYPOP

bool DependencyParser::parseTournament(Tree *tree) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  const int size = static_cast<int>(tree->chunk_size());
  CHECK_DIE(size >= 2);

  double score = 0.0;
  Hypothesis *hypo = data->hypothesis();
  CHECK_DIE(hypo);
  hypo->init(size);

  if (action_mode() == TRAINING_MODE) {
    for (int src = size - 2; src >= 0; --src) {
      const int head = tree->chunk(src)->link;
      if (head == -1) {   // dependency is unknown.
        continue;
      }
      for (int dst = src + 1; dst <= head - 1; ++dst) {
        estimate(tree, src, dst, head, &score);
      }
      for (int dst = head + 1; dst <= size - 1; ++dst) {
        estimate(tree, src, head, dst, &score);
      }
      hypo->children[head].push_back(src);
    }
  } else {
    for (int i = 0; i < size; ++i) {
      hypo->head[i] = i + 1;
    }
    CHECK_DIE(hypo->head[size - 1] == size);
    const std::vector<int> &head = hypo->head;
    for (int src = size - 2; src >= 0; --src) {
      int h = src + 1;
      int dst = head[h];
      while (dst != size) {
        if (estimate(tree, src, h, dst, &score)) {
          h = dst;
        }
        dst = head[dst];
      }
      hypo->head[src] = h;
      hypo->score[src] = std::fabs(score);
      hypo->children[h].push_back(src);
    }

    // propagete hypothesis.
    for (int src = 0; src < size - 1; ++src) {
      Chunk *chunk = tree->mutable_chunk(src);
      CHECK_DIE(chunk != 0);
      chunk->link = hypo->head[src];
      chunk->score = hypo->score[src];
    }
  }

  tree->set_output_layer(OUTPUT_DEP);
  return true;
}

bool DependencyParser::parse(Tree *tree) const {
  if (!tree->allocator()->dependency_parser_data) {
    tree->allocator()->dependency_parser_data
        = new DependencyParserData;
    CHECK_DIE(tree->allocator()->dependency_parser_data);
  }

  if (tree->chunk_size() == 0) {
    return true;
  }

  if (tree->chunk_size() == 1) {
    tree->mutable_chunk(tree->chunk_size() - 1)->link = -1;
    tree->mutable_chunk(tree->chunk_size() - 1)->score = 0.0;
    return true;
  }

  // make features
  build(tree);

  switch (parsing_algorithm_) {
    case SHIFT_REDUCE:
      return parseShiftReduce(tree);
    case TOURNAMENT:
      return parseTournament(tree);
    default:
      CHECK_DIE(true) << "Unknown parsing model: " << parsing_algorithm_;
      return false;
  }
  return false;
}
}  // namespace CaboCha
