// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: chunker.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <crfpp.h>
#include <cstdio>
#include <iterator>
#include "utils.h"
#include "cabocha.h"
#include "chunker.h"
#include "param.h"
#include "common.h"

namespace CaboCha {

Chunker::Chunker(): tagger_(0) {}
Chunker::~Chunker() { close(); }

bool Chunker::open(const Param &param) {
  close();

  if (action_mode() == PARSING_MODE) {
    const std::string filename = param.get<std::string>("chunker-model");
    std::vector<const char*> argv;
    argv.push_back(param.program_name());
    argv.push_back("-m");
    argv.push_back(filename.c_str());
    tagger_ = crfpp_new(argv.size(),
                        const_cast<char **>(&argv[0]));
    CHECK_FALSE(tagger_) << crfpp_strerror(tagger_);
    CHECK_FALSE(crfpp_ysize(tagger_) == 2);
    CHECK_FALSE(crfpp_xsize(tagger_) == 2);
    CHECK_FALSE(std::strcmp("B", crfpp_yname(tagger_, 0)) == 0);
    CHECK_FALSE(std::strcmp("I", crfpp_yname(tagger_, 1)) == 0);
  }

  return true;
}

void Chunker::close() {
  crfpp_destroy(tagger_);
  tagger_ = 0;
}

bool Chunker::parse(Tree *tree) {
  CHECK_FALSE(tree);

  if (action_mode() == PARSING_MODE) {
    CHECK_FALSE(tagger_);
    crfpp_clear(tagger_);
  }

  const size_t size = tree->token_size();
  std::string tmp;
  for (size_t i = 0; i < size; ++i) {
    feature_.clear();
    const Token *token = tree->token(i);
    feature_.push_back(token->normalized_surface);
    if (tree->posset() == IPA || tree->posset() == UNIDIC) {
      concat_feature(token, 4, &tmp);
    } else if (tree->posset() == JUMAN) {
      concat_feature(token, 2, &tmp);
    }
    const char *pos = tree->strdup(tmp.c_str());
    feature_.push_back(pos);

    if (action_mode() == PARSING_MODE) {
      crfpp_add2(tagger_, feature_.size(), (const char **)&feature_[0]);
    } else {
      std::copy(feature_.begin(), feature_.end(),
                std::ostream_iterator<const char *> (*stream(), " "));
      const char c = (i == 0 || token->chunk) ? 'B' : 'I';
      *stream() << c << std::endl;
    }
  }

  if (action_mode() == PARSING_MODE) {
    CHECK_FALSE(crfpp_parse(tagger_));

    tree->clear_chunk();
    Chunk *old_chunk = 0;

    for (size_t i = 0; i < size; ++i) {
      Token *token = tree->mutable_token(i);
      if (!token->chunk &&
          (i == 0 ||
           std::strcmp(crfpp_y2(tagger_, i), "B") == 0)) {
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
    *stream() << std::endl;
  }

  tree->set_output_layer(OUTPUT_CHUNK);

  return true;
}
}
