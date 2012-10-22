// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: dep_learner.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "analyzer.h"
#include "dep.h"
#include "common.h"
#include "freelist.h"
#include "param.h"
#include "scoped_ptr.h"
#include "selector.h"
#include "svm.h"
#include "svm_learn.h"
#include "timer.h"
#include "tree_allocator.h"
#include "utils.h"

namespace CaboCha {

bool DependencyTrainingWithSVM(const char *train_file,
                               const char *model_file,
                               const char *prev_model_file,
                               CharsetType charset,
                               PossetType posset,
                               int parsing_algorithm,
                               double cost) {
  CHECK_DIE(cost > 0.0) << "cost must be positive value";
  CHECK_DIE(parsing_algorithm == CABOCHA_TOURNAMENT ||
            parsing_algorithm == CABOCHA_SHIFT_REDUCE);

  const std::string str_train_file = std::string(model_file) + ".str";

  scoped_fixed_array<char, BUF_SIZE * 32> buf;
  scoped_fixed_array<char *, BUF_SIZE> column;

  {
    progress_timer pg;
    Param param;

    std::ifstream ifs(WPATH(train_file));
    CHECK_DIE(ifs) << "no such file or directory: " << train_file;

    std::ofstream ofs(WPATH(str_train_file.c_str()));
    CHECK_DIE(ofs) << "permission denied: " << str_train_file;

    DependencyParser *dependency_parser = new DependencyParser;
    dependency_parser->set_parsing_algorithm(parsing_algorithm);

    scoped_ptr<Analyzer> analyzer(dependency_parser);
    scoped_ptr<Analyzer> selector(new Selector);
    Tree tree;
    std::cout << "reading training data: " << std::flush;

    tree.set_charset(charset);
    tree.set_posset(posset);
    tree.allocator()->set_stream(&ofs);

    analyzer->set_charset(charset);
    analyzer->set_posset(posset);
    analyzer->set_action_mode(TRAINING_MODE);
    selector->set_charset(charset);
    selector->set_posset(posset);
    selector->open(param);

    size_t line = 0;
    std::string str;
    while (ifs) {
      CHECK_DIE(read_sentence(&ifs, &str, INPUT_CHUNK));
      if (str.empty()) {
        break;
      }
      CHECK_DIE(tree.read(str.c_str(), str.size(),
                          INPUT_CHUNK)) << "cannot parse sentence";
      CHECK_DIE(selector->parse(&tree)) << selector->what();
      CHECK_DIE(analyzer->parse(&tree)) << analyzer->what();
      CHECK_DIE(!tree.empty()) << "[" << str << "]";
      if (++line % 100 == 0) {
        std::cout << line << ".. " << std::flush;
      }
    }
    std::cout << "\nDone! ";
  }

  scoped_ptr<SVMModel> model(
      SVMSolver::learn(str_train_file.c_str(),
                       prev_model_file, cost));
  CHECK_DIE(model.get());

  model->set_param("parsing_algorithm", parsing_algorithm);
  model->set_param("charset", encode_charset(charset));
  model->set_param("posset",  encode_posset(posset));
  model->set_param("type", "dep");

  Unlink(str_train_file.c_str());

  return model->save(model_file);
}
}
