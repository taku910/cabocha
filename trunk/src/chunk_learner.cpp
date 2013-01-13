// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: chunk_learner.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <crfpp.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include "cabocha.h"
#include "chunker.h"
#include "common.h"
#include "ne.h"
#include "scoped_ptr.h"
#include "utils.h"
#include "timer.h"
#include "tree_allocator.h"

namespace {
const char chunk_crfpp_template[] =
    "U00:%x[-2,0]\n"
    "U01:%x[-1,0]\n"
    "U02:%x[0,0]\n"
    "U03:%x[1,0]\n"
    "U04:%x[2,0]\n"
    "U05:%x[-1,0]/%x[0,0]\n"
    "U06:%x[0,0]/%x[1,0]\n"
    "U10:%x[-2,1]\n"
    "U11:%x[-1,1]\n"
    "U12:%x[0,1]\n"
    "U13:%x[1,1]\n"
    "U14:%x[2,1]\n"
    "U15:%x[-2,1]/%x[-1,1]\n"
    "U16:%x[-1,1]/%x[0,1]\n"
    "U17:%x[0,1]/%x[1,1]\n"
    "U18:%x[1,1]/%x[2,1]\n"
    "U20:%x[-2,1]/%x[-1,1]/%x[0,1]\n"
    "U21:%x[-1,1]/%x[0,1]/%x[1,1]\n"
    "U22:%x[0,1]/%x[1,1]/%x[2,1]\n"
    "B\n";

const char ne_crfpp_template[] =
    "U00:%x[-2,0]\n"
    "U01:%x[-1,0]\n"
    "U02:%x[0,0]\n"
    "U03:%x[1,0]\n"
    "U04:%x[2,0]\n"
    "U05:%x[-1,0]/%x[0,0]\n"
    "U06:%x[0,0]/%x[1,0]\n"
    "U10:%x[-2,1]\n"
    "U11:%x[-1,1]\n"
    "U12:%x[0,1]\n"
    "U13:%x[1,1]\n"
    "U14:%x[2,1]\n"
    "U15:%x[-2,1]/%x[-1,1]\n"
    "U16:%x[-1,1]/%x[0,1]\n"
    "U17:%x[0,1]/%x[1,1]\n"
    "U18:%x[1,1]/%x[2,1]\n"
    "U20:%x[-2,1]/%x[-1,1]/%x[0,1]\n"
    "U21:%x[-1,1]/%x[0,1]/%x[1,1]\n"
    "U22:%x[0,1]/%x[1,1]/%x[2,1]\n"
    "U30:%x[-2,2]\n"
    "U31:%x[-1,2]\n"
    "U32:%x[0,2]\n"
    "U33:%x[1,2]\n"
    "U34:%x[2,2]\n"
    "U35:%x[-2,2]/%x[-1,2]\n"
    "U36:%x[-1,2]/%x[0,2]\n"
    "U37:%x[0,2]/%x[1,2]\n"
    "U38:%x[1,2]/%x[2,2]\n"
    "U40:%x[-2,2]/%x[-1,2]/%x[0,2]\n"
    "U41:%x[-1,2]/%x[0,2]/%x[1,2]\n"
    "U42:%x[0,2]/%x[1,2]/%x[2,2]\n"
    "U50:%x[0,1]/%x[0,2]\n"
    "B\n";
}

namespace CaboCha {
namespace {
bool runChunkingTrainingWithCRFPP(ParserType type,
                                  const char *train_file,
                                  const char *model_file,
                                  const char *prev_model_file,
                                  CharsetType charset,
                                  PossetType posset,
                                  double cost,
                                  int freq,
                                  FeatureExtractorInterface *feature_extractor) {
  CHECK_DIE(freq >= 1);
  CHECK_DIE(cost > 0.0);
  const char *template_str = type == TRAIN_CHUNK ?
      chunk_crfpp_template : ne_crfpp_template;

  const std::string tmp_train_file = std::string(model_file) + ".crfpp";
  const std::string templ_file = std::string(model_file) + ".templ";

  {
    progress_timer pg;
    std::ifstream ifs(WPATH(train_file));
    CHECK_DIE(ifs) << "no such file or directory: " << train_file;

    std::ofstream ofs(WPATH(tmp_train_file.c_str()));
    CHECK_DIE(ofs) << "permission denied: " << tmp_train_file;

    scoped_ptr<Analyzer> analyzer(0);
    Tree tree;
    std::cout << "reading training data: " << std::flush;

    InputLayerType input = INPUT_POS;
    if (type == TRAIN_NE) {
      analyzer.reset(new NE);
      input = INPUT_POS;
    } else if (type == TRAIN_CHUNK) {
      analyzer.reset(new Chunker);
      input = INPUT_CHUNK;
    } else {
      CHECK_DIE(false) << "unknown type: " << type;
    }

    tree.set_charset(charset);
    tree.set_posset(posset);
    tree.allocator()->set_stream(&ofs);

    analyzer->set_charset(charset);
    analyzer->set_posset(posset);
    analyzer->set_action_mode(TRAINING_MODE);

    size_t line = 0;
    std::string str;
    while (ifs) {
      CHECK_DIE(read_sentence(&ifs, &str, INPUT_CHUNK));
      CHECK_DIE(tree.read(str.c_str(), str.size(),
                          input)) << "cannot parse sentence";
      CHECK_DIE(analyzer->parse(&tree)) << analyzer->what();
      if (tree.empty()) {
        continue;
      }
      if (++line % 100 == 0) {
        std::cout << line << ".. " << std::flush;
      }
    }
    std::cout << "\nDone! ";
  }

  // call CRF++
  {
    {
      std::ofstream ofs(WPATH(templ_file.c_str()));
      CHECK_DIE(ofs) << "permission denied: " << templ_file;
      ofs << template_str;
    }

    std::vector<const char *> argv;
    argv.push_back("cabocha_learn");

    char buf1[256];
    snprintf(buf1, sizeof(buf1), "--freq=%d", freq);
    argv.push_back(buf1);

    char buf2[256];
    snprintf(buf2, sizeof(buf2), "--cost=%f", cost);
    argv.push_back(buf2);
    argv.push_back("-t");

    argv.push_back(templ_file.c_str());
    argv.push_back(tmp_train_file.c_str());
    argv.push_back(model_file);

    CHECK_DIE(0 == crfpp_learn(static_cast<int>(argv.size()),
                               const_cast<char **>(&argv[0])))
        << "crfpp_learn execution error";

    Unlink(tmp_train_file.c_str());
    Unlink(templ_file.c_str());
  }

  return true;
}
}  // namespace

bool runChunkingTraining(const char *train_file,
                         const char *model_file,
                         const char *prev_model_file,
                         CharsetType charset,
                         PossetType posset,
                         double cost,
                         int freq,
                         FeatureExtractorInterface *feature_extractor) {
  return runChunkingTrainingWithCRFPP(TRAIN_CHUNK,
                                      train_file,
                                      model_file,
                                      prev_model_file,
                                      charset,
                                      posset,
                                      cost,
                                      freq,
                                      feature_extractor);
}

bool runNETraining(const char *train_file,
                   const char *model_file,
                   const char *prev_model_file,
                   CharsetType charset,
                   PossetType posset,
                   double cost,
                   int freq,
                   FeatureExtractorInterface *feature_extractor) {
  return runChunkingTrainingWithCRFPP(TRAIN_NE,
                                      train_file,
                                      model_file,
                                      prev_model_file,
                                      charset,
                                      posset,
                                      cost,
                                      freq,
                                      feature_extractor);
}
}
