// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: chunker.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <crfpp.h>
#include <cstdio>
#include <iterator>
#include "cabocha.h"
#include "chunker.h"
#include "common.h"
#include "param.h"
#include "tree_allocator.h"
#include "utils.h"

namespace CaboCha {

Chunker::Chunker(): model_(0) {}
Chunker::~Chunker() { close(); }

bool Chunker::open(const Param &param) {
  close();

  if (action_mode() == PARSING_MODE) {
    const std::string filename = param.get<std::string>("chunker-model");
    std::vector<const char *> argv;
    argv.push_back(param.program_name());
    argv.push_back("-m");
    argv.push_back(filename.c_str());
    model_ = crfpp_model_new(argv.size(),
                             const_cast<char **>(&argv[0]));
    CHECK_FALSE(model_) << crfpp_model_strerror(model_);
    //    CHECK_FALSE(crfpp_ysize(tagger_) == 2);
    //    CHECK_FALSE(crfpp_xsize(tagger_) == 2);
    //    CHECK_FALSE(std::strcmp("B", crfpp_yname(tagger_, 0)) == 0);
    //    CHECK_FALSE(std::strcmp("I", crfpp_yname(tagger_, 1)) == 0);
  }

  return true;
}

void Chunker::close() {
  crfpp_model_destroy(model_);
  model_ = 0;
}

bool Chunker::parse(Tree *tree) const {
  TreeAllocator *allocator = tree->allocator();
  CHECK_TREE_FALSE(allocator);

  if (action_mode() == PARSING_MODE) {
    CHECK_TREE_FALSE(model_);
    if (!allocator->crfpp_chunker) {
      allocator->crfpp_chunker = crfpp_model_new_tagger(model_);
      CHECK_TREE_FALSE(allocator->crfpp_chunker);
    }
    crfpp_set_model(allocator->crfpp_chunker, model_);
    crfpp_clear(allocator->crfpp_chunker);
  }

  const size_t size = tree->token_size();
  std::string tmp;
  for (size_t i = 0; i < size; ++i) {
    allocator->feature.clear();
    const Token *token = tree->token(i);
    allocator->feature.push_back(token->normalized_surface);
    if (tree->posset() == IPA || tree->posset() == UNIDIC) {
      concat_feature(token, 4, &tmp);
    } else if (tree->posset() == JUMAN) {
      concat_feature(token, 2, &tmp);
    }
    const char *pos = tree->strdup(tmp.c_str());
    allocator->feature.push_back(pos);

    if (action_mode() == PARSING_MODE) {
      crfpp_add2(allocator->crfpp_chunker,
                 allocator->feature.size(),
                 (const char **)&(allocator->feature[0]));
    } else {
      std::copy(allocator->feature.begin(), allocator->feature.end(),
                std::ostream_iterator<const char *>(
                    *(allocator->stream()), " "));
      const char c = (i == 0 || token->chunk) ? 'B' : 'I';
      *(allocator->stream()) << c << std::endl;
    }
  }

  if (action_mode() == PARSING_MODE) {
    CHECK_TREE_FALSE(crfpp_parse(allocator->crfpp_chunker));

    tree->clear_chunk();
    Chunk *old_chunk = 0;

    for (size_t i = 0; i < size; ++i) {
      Token *token = tree->mutable_token(i);
      if (!token->chunk &&
          (i == 0 ||
           std::strcmp(crfpp_y2(allocator->crfpp_chunker, i), "B") == 0)) {
        token->chunk = tree->add_chunk();
      }

      if (token->chunk) {
        token->chunk->token_pos = i;
        old_chunk = token->chunk;
      }

      if (old_chunk) {
        old_chunk->token_size++;
      }
    }
  } else {
    *(allocator->stream()) << std::endl;
  }

  tree->set_output_layer(OUTPUT_CHUNK);

  return true;
}
}
