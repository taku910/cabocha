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
#include "dep.h"
#include "cabocha.h"
#include "common.h"
#include "param.h"
#include "svm.h"
#include "timer.h"
#include "tree_allocator.h"
#include "utils.h"

namespace CaboCha {

namespace {
const size_t kMaxGapSize = 7;
}

DependencyParser::DependencyParser() : svm_(0) {}
DependencyParser::~DependencyParser() {}

bool DependencyParser::open(const Param &param) {
  close();
  if (action_mode() == PARSING_MODE) {
    const std::string filename = param.get<std::string>("parser-model");
    bool failed = false;
    svm_.reset(new SVM);
    if (!svm_->open(filename.c_str())) {
      svm_.reset(new SVMTest);
      if (!svm_->open(filename.c_str())) {
        failed = true;
      }
    }
    CHECK_FALSE(!failed) << "no such file or directory: " << filename;
  }
  return true;
}

void DependencyParser::close() {
  svm_.reset(0);
}

void DependencyParser::build(Tree *tree) const {
  const size_t size  = tree->chunk_size();

  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  data->static_feature.clear();
  data->static_feature.resize(size);

  data->dyn_b_feature.clear();
  data->dyn_b_feature.resize(size);
  data->dyn_b.clear();
  data->dyn_b.resize(size);

  data->dyn_a_feature.clear();
  data->dyn_a_feature.resize(size);
  data->dyn_a.clear();
  data->dyn_a.resize(size);

  data->gap.clear();
  data->gap_list.clear();
  data->gap_list.resize(size);

  for (size_t i = 0; i < tree->chunk_size(); ++i) {
    Chunk *chunk = tree->mutable_chunk(i);
    for (size_t k = 0; k < chunk->feature_list_size; ++k) {
      char *feature = const_cast<char *>(chunk->feature_list[k]);
      switch (feature[0]) {
        case 'F': data->static_feature[i].push_back(feature); break;
        case 'G': data->gap_list[i].push_back(feature);       break;
        case 'A': data->dyn_a_feature[i].push_back(feature);  break;
        case 'B': data->dyn_b_feature[i].push_back(feature);  break;
        default:
          break;
      }
    }
  }

  // make gap features
  data->gap.resize(size * (size + 3) / 2 + 1);

  for (size_t k = 0; k < size; k++) {
    for (size_t i = 0; i < k; i++) {
      for (size_t j = k + 1; j < size; j++) {
        std::copy(data->gap_list[k].begin(), data->gap_list[k].end(),
                  std::back_inserter(data->gap[j * (j + 1) / 2 + i]));
      }
    }
  }

  data->results.resize(size);
  for (size_t i = 0; i < data->results.size(); ++i) {
    data->results[i].score = 0.0;
    data->results[i].link = -1;
  }
}

bool DependencyParser::estimate(const Tree *tree,
                                int src, int dst,
                                double *score) const {
  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  if (action_mode() == PARSING_MODE &&
      data->results[src].link != -1) {
    return tree->chunk(src)->link == dst;
  }

  data->fpset.clear();
  const int dist = dst - src;
  if (dist == 1) {
    data->fpset.insert("DIST:1");
  } else if (dist >= 2 && dist <= 5) {
    data->fpset.insert("DIST:2-5");
  } else {
    data->fpset.insert("DIST:6-");
  }

  // static feature
  for (size_t i = 0; i < data->static_feature[src].size(); ++i) {
    data->static_feature[src][i][0] = 'f';
    data->fpset.insert(data->static_feature[src][i]);
  }

  for (size_t i = 0; i < data->static_feature[dst].size(); ++i) {
    data->static_feature[dst][i][0] = 'F';
    data->fpset.insert(data->static_feature[dst][i]);
  }

  size_t max_gap = 0;

  // gap feature
  const int k = dst * (dst + 1) / 2 + src;
  max_gap = std::min(data->gap[k].size(), kMaxGapSize);
  for (size_t i = 0; i < max_gap; ++i) {
    data->fpset.insert(data->gap[k][i]);
  }

  // dynamic features
  max_gap = std::min(data->dyn_a[dst].size(), kMaxGapSize);
  for (size_t i = 0; i < max_gap; ++i) {
    data->dyn_a[dst][i][0] = 'A';
    data->fpset.insert(data->dyn_a[dst][i]);
  }

  max_gap = std::min(data->dyn_a[src].size(), kMaxGapSize);
  for (size_t i = 0; i < max_gap; ++i) {
    data->dyn_a[src][i][0] = 'a';
    data->fpset.insert(data->dyn_a[src][i]);
  }

  max_gap = std::min(data->dyn_b[dst].size(), kMaxGapSize);
  for (size_t i = 0; i < max_gap; ++i) {
    data->dyn_b[dst][i][0] = 'B';
    data->fpset.insert(data->dyn_b[dst][i]);
  }

  const size_t fsize = data->fpset.size();
  std::copy(data->fpset.begin(), data->fpset.end(), data->fp);

  TreeAllocator *allocator = tree->allocator();

  if (action_mode() == PARSING_MODE) {
    *score = svm_->classify(fsize, const_cast<char **>(data->fp));
    return *score > 0;
  } else {
    const bool isdep = (tree->chunk(src)->link == dst);
    *(allocator->stream()) << (isdep ? "+1" : "-1");
    for (size_t i = 0; i < fsize; ++i) {
      *(allocator->stream()) << ' ' << data->fp[i];
    }
    *(allocator->stream()) << std::endl;
    return isdep;
  }

  return false;
}

bool DependencyParser::parse(Tree *tree) const {
  if (!tree->allocator()->dependency_parser_data) {
    tree->allocator()->dependency_parser_data
        = new DependencyParserData;
    CHECK_DIE(tree->allocator()->dependency_parser_data);
  }

  DependencyParserData *data
      = tree->allocator()->dependency_parser_data;
  CHECK_DIE(data);

  const int size = static_cast<int>(tree->chunk_size());
  if (size == 0) {
    return true;
  }

  build(tree);
  std::vector<int> dead(size);
  std::vector<int> alive(size);

  std::fill(dead.begin(), dead.end(), 0);

  // main loop
  while (true) {
    alive.clear();
    for (size_t i = 0; i < dead.size(); ++i) {
      if (!dead[i]) {
        alive.push_back(i);
      }
    }

    if (alive.size() <= 1) {
      break;
    }

    // if previous chunk does NOT depend on the next true;
    bool prev = false;

    // if current seeking does NOT change the status, true
    bool change = false;

    // size of alive, ignore the last chunk
    const int lsize  = static_cast<int>(alive.size() - 1);

    // last index of alive, ignore tha last chunk
    const int lindex = static_cast<int>(alive.size() - 2);

    for (int i = 0; i < lsize; ++i) {
      const int src   = alive[i];
      const int dst   = alive[i + 1];
      bool      isdep = true;
      double    score = 0.0;

      if (src != size - 2 && data->results[src].link != dst) {
        isdep = estimate(tree, src, dst, &score);
      }

      // force to change TRUE
      if (i == lindex && !change) {
        isdep = true;
      }

      // OK depend
      if (isdep) {
        std::copy(data->dyn_b_feature[dst].begin(),
                  data->dyn_b_feature[dst].end(),
                  std::back_inserter(data->dyn_b[src]));
        std::copy(data->dyn_a_feature[src].begin(),
                  data->dyn_a_feature[src].end(),
                  std::back_inserter(data->dyn_a[dst]));
        data->results[src].link = dst;
        data->results[src].score = score;
        if (!dead[src]) {
          change = true;
        }
        if (!prev) {
          dead[src] = true;
        }
      }

      prev = isdep;
    }
  }

  for (size_t i = 0; i < data->results.size(); ++i) {
    Chunk *chunk = tree->mutable_chunk(i);
    chunk->link = data->results[i].link;
    chunk->score = data->results[i].score;
  }

  tree->set_output_layer(OUTPUT_DEP);

  return true;
}

#if 0
// Sassano's algorithm
bool DependencyParser::parse(Tree *tree) {
#define MYPOP(agenda, n) do {                   \
    if (agenda.empty()) {                       \
      n = -1;                                   \
    } else {                                    \
      n = agenda.top();                         \
      agenda.pop();                             \
    }                                           \
  } while (0)

  const int size = static_cast<int>(tree->chunk_size());
  build(tree);

  std::stack<int> agenda;
  double score = 0.0;
  agenda.push(0);
  for (int dst = 1; dst < size; ++dst) {
    int src = 0;
    MYPOP(agenda, src);
    while (src != -1 && estimate(tree, src, dst, &score)) {
      Chunk *chunk = tree->mutable_chunk(src);
      chunk->link = dst;
      chunk->score = score;

      // copy dynamic feature
      std::copy(dyn_b_feature_[dst].begin(),
                dyn_b_feature_[dst].end(),
                std::back_inserter(dyn_b_[src]));
      std::copy(dyn_a_feature_[src].begin(),
                dyn_a_feature_[src].end(),
                std::back_inserter(dyn_a_[dst]));

      MYPOP(agenda, src);
    }
    if (src != -1) agenda.push(src);
    agenda.push(dst);
  }

  tree->set_output_layer(OUTPUT_DEP);
  return true;
#undef MYPOP
}
#endif
}
