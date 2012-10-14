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

#define INIT_FEATURE(array) do { \
    array.clear(); array.resize(size); } while (false)

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
  INIT_FEATURE(data->children);

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
  for (size_t i = 0; i < data->children[dst].size(); ++i) {
    const int child = data->children[dst][i];
    for (size_t j = 0; j < data->dynamic_feature[child].size(); ++j) {
      data->dynamic_feature[child][j][0] = 'A';
      data->fpset.insert(data->dynamic_feature[child][j]);
    }
  }

  for (size_t i = 0; i < data->children[src].size(); ++i) {
    const int child = data->children[src][i];
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

// Sassano's algorithm
#define MYPOP(agenda, n) do {                   \
    if (agenda.empty()) {                       \
      n = -1;                                   \
    } else {                                    \
      n = agenda.top();                         \
      agenda.pop();                             \
    }                                           \
  } while (0)

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
  build(tree);

  std::stack<int> agenda;
  double score = 0.0;
  agenda.push(0);
  for (int dst = 1; dst < size; ++dst) {
    int src = 0;
    MYPOP(agenda, src);
    // if agenda is empty, src == -1.
    while (src != -1 &&
           (dst == size - 1 || estimate(tree, src, dst, &score))) {
      Chunk *chunk = tree->mutable_chunk(src);
      chunk->link = dst;
      chunk->score = score;

      // store children for dynamic_features
      data->children[dst].push_back(src);

      MYPOP(agenda, src);
    }
    if (src != -1) {
      agenda.push(src);
    }
    agenda.push(dst);
  }

  tree->set_output_layer(OUTPUT_DEP);
  return true;
}
#undef MYPOP
}
