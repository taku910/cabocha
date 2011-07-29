// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: dep_learner.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include "analyzer.h"
#include "selector.h"
#include "dep.h"
#include "param.h"
#include "common.h"
#include "freelist.h"
#include "scoped_ptr.h"
#include "utils.h"
#include "timer.h"
#include "svm_learn.h"

namespace CaboCha {

bool DependencyTrainingWithSVM(const char *train_file,
                               const char *model_file,
                               CharsetType charset,
                               PossetType posset,
                               int degree,
                               double cost,
                               double cache_size) {
  CHECK_DIE(cache_size >= 40.0) << "too small memory size: " << cache_size;
  CHECK_DIE(cost > 0.0) << "cost must be positive value";
  CHECK_DIE(degree >= 1 && degree <= 3) << "degree must be 1<=degree<=3";

  std::map<std::string, int> dic;
  const std::string str_train_file = std::string(model_file) + ".str";
  const std::string id_train_file = std::string(model_file) + ".id";
  const std::string svm_learn_model_file = std::string(model_file) + ".model";

  {
    progress_timer pg;
    Param param;

    std::ifstream ifs(train_file);
    CHECK_DIE(ifs) << "no such file or directory: " << train_file;

    std::ofstream ofs(str_train_file.c_str());
    CHECK_DIE(ofs) << "permission denied: " << str_train_file;

    scoped_ptr<Analyzer> analyzer(new DependencyParser);
    scoped_ptr<Analyzer> selector(new Selector);
    Tree tree;
    std::cout << "reading training data: " << std::flush;

    tree.set_charset(charset);
    tree.set_posset(posset);
    analyzer->set_charset(charset);
    analyzer->set_posset(posset);
    analyzer->set_action_mode(TRAINING_MODE);
    analyzer->set_stream(&ofs);
    selector->set_charset(charset);
    selector->set_posset(posset);
    selector->set_stream(&ofs);
    selector->open(param);

    size_t line = 0;
    std::string str;
    while (ifs) {
      CHECK_DIE(read_sentence(&ifs, &str, INPUT_CHUNK));
      CHECK_DIE(tree.read(str.c_str(), str.size(),
                          INPUT_CHUNK)) << "cannot parse sentence";
      CHECK_DIE(selector->parse(&tree)) << selector->what();
      CHECK_DIE(analyzer->parse(&tree)) << analyzer->what();
      if (tree.empty()) continue;
      if (++line % 100 == 0)
        std::cout << line << ".. " << std::flush;
    }
    std::cout << "\nDone! ";
  }

  // read training data and make dic
  {
    std::ifstream ifs(str_train_file.c_str());
    CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;
    char buf[8192 * 32];
    char *column[BUF_SIZE];
    while (ifs.getline(buf, sizeof(buf))) {
      const size_t size = tokenize(buf, " ", column, BUF_SIZE);
      CHECK_DIE(size >= 2);
      for (size_t i = 1; i < size; ++i) {
        dic[std::string(column[i])]++;
      }
    }
  }

  // make dic -> id table
  {
    std::vector<std::pair<int, std::string> > freq;
    for (std::map<std::string, int>::const_iterator it = dic.begin();
         it != dic.end(); ++it) {
      freq.push_back(std::make_pair(it->second, it->first));
    }
    //      std::sort(freq.begin(), freq.end(),
    //  std::greater<std::pair<int, std::string> >());
    std::sort(freq.begin(), freq.end());
    int id = 0;
    for (size_t i = 0; i < freq.size(); ++i) {
      dic[freq[i].second] = id;
      ++id;
    }
  }

  std::vector<int *> x;
  std::vector<double> y;
  FreeList<int> feature_freelist(8192);

  {
    std::set<std::string> dup;
    std::ifstream ifs(str_train_file.c_str());
    CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;

    const std::string model_debug_file = std::string(model_file) + ".svm";
    std::ofstream ofs(model_debug_file.c_str());

    char buf[8192 * 32];
    char *column[BUF_SIZE];
    while (ifs.getline(buf, sizeof(buf))) {
      if (dup.find(buf) != dup.end())
        continue;
      dup.insert(buf);
      const size_t size = tokenize(buf, " ", column, BUF_SIZE);
      CHECK_DIE(size >= 2);
      std::vector<int> tmp;
      for (size_t i = 1; i < size; ++i) {
        tmp.push_back(dic[std::string(column[i])]);
      }
      std::sort(tmp.begin(), tmp.end());

      ofs << std::atof(column[0]);
      for (size_t i = 0; i < tmp.size(); ++i) {
        ofs << " " << tmp[i] << ":1";
      }
      ofs << std::endl;

      tmp.push_back(-1);
      int *fp = feature_freelist.alloc(tmp.size());
      std::copy(tmp.begin(), tmp.end(), fp);
      x.push_back(fp);
      y.push_back(std::atof(column[0]));
    }
    ::unlink(str_train_file.c_str());
  }

  CHECK_DIE(x.size() >= 2) << "training data is too small";
  CHECK_DIE(x.size() == y.size());

  scoped_ptr<SVMModel> model;
  model.reset(SVMSolver::learn(y.size(),
                               &y[0],
                               &x[0],
                               cost,
                               degree,
                               cache_size));
  CHECK_DIE(model.get());

  {
    std::ofstream ofs(model_file);
    for (std::map<std::string, int>::const_iterator it = dic.begin();
         it != dic.end(); ++it) {
      ofs << it->second << ' ' << it->first << std::endl;
    }
    ofs << std::endl;
    ofs << model->degree() << std::endl;
    ofs << model->bias() << std::endl;
    for (size_t i = 0; i < model->size(); ++i) {
      ofs << model->alpha(i);
      for (const int *fp = model->x(i); *fp >= 0; ++fp) {
        ofs << ' ' << *fp;
      }
      ofs << std::endl;
    }
  }


  /*
  // make training data for svm_learn
  {
  std::set<std::string> dup;
  std::ifstream ifs(str_train_file.c_str());
  CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;

  std::ofstream ofs(id_train_file.c_str());
  CHECK_DIE(ofs) << "permission denied: " << id_train_file;

  char buf[8192 * 32];
  char *column[BUF_SIZE];
  while (ifs.getline(buf, sizeof(buf))) {
  const size_t size = tokenize(buf, " ", column, BUF_SIZE);
  CHECK_DIE(size >= 2);
  std::vector<int> tmp;
  for (size_t i = 1; i < size; ++i) {
  tmp.push_back(dic[std::string(column[i])]);
  }
  std::sort(tmp.begin(), tmp.end());

  std::stringstream sofs;
  sofs << column[0];
  for (size_t i = 0; i < tmp.size(); ++i) {
  sofs << ' ' << tmp[i] + 1 << ":1";
  }
  const std::string &line = sofs.str();
  std::set<std::string>::const_iterator it = dup.find(line);
  if (it == dup.end()) {
  ofs << line << std::endl;
  dup.insert(line);
  }
  }
  }

  // call svm_learn
  {
  progress_timer pg;
  char argv[8192 * 32];
  const std::string svm_learn =
  svm_learn_program ? svm_learn_program : "svm_learn";
  std::sprintf(argv,
  "%s -t1 -d%d -c%f -m%f \"%s\" \"%s\"",
  svm_learn.c_str(),
  degree,
  cost,
  cache_size,
  id_train_file.c_str(),
  svm_learn_model_file.c_str());
  std::cout << argv << std::endl;
  system(argv);
  }

  // make
  {
  std::cout << "Converting "
  << svm_learn_model_file
  << " to "
  << model_file << " ..." << std::flush;

  std::ifstream ifs(svm_learn_model_file.c_str());
  CHECK_DIE(ifs) << "no such file or directory: " << id_train_file;
  std::ofstream ofs(model_file);

  CHECK_DIE(ofs) << "permission denied: " << train_file;

  for (std::map<std::string, int>::const_iterator it = dic.begin();
  it != dic.end(); ++it) {
  ofs << it->second << ' ' << it->first << std::endl;
  }
  ofs << std::endl;
  ofs << degree << std::endl;

  std::string line;
  bool emit_bias = false;
  while (std::getline(ifs, line)) {
  if (line.find("# threshold b") != std::string::npos) {
  const double bias = std::atof(line.c_str());
  ofs << bias << std::endl;
  emit_bias = true;
  break;
  }
  }
  CHECK_DIE(emit_bias) << "bias value is not found";

  char buf[8192 * 32];
  char *column[BUF_SIZE];
  while (ifs.getline(buf, sizeof(buf))) {
  const size_t size = tokenize(buf, " ", column, BUF_SIZE);
  ofs << column[0];
  for (size_t i = 1; i < size; ++i) {
  const int id = std::atoi(column[i]) - 1;
  CHECK_DIE(id >= 0);
  ofs << ' ' << id;
  }
  ofs << std::endl;
  }
  }

  ::unlink(str_train_file.c_str());
  ::unlink(id_train_file.c_str());
  ::unlink(svm_learn_model_file.c_str());

  */

  std::cout << "\nDone! ";

  return true;
}
}
