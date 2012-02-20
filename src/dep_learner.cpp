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
#include "svm_learn.h"
#include "timer.h"
#include "tree_allocator.h"
#include "utils.h"

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

    std::ifstream ifs(WPATH(train_file));
    CHECK_DIE(ifs) << "no such file or directory: " << train_file;

    std::ofstream ofs(WPATH(str_train_file.c_str()));
    CHECK_DIE(ofs) << "permission denied: " << str_train_file;

    scoped_ptr<Analyzer> analyzer(new DependencyParser);
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
    std::ifstream ifs(WPATH(str_train_file.c_str()));
    CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;
    scoped_fixed_array<char, BUF_SIZE * 32> buf;
    scoped_fixed_array<char *, BUF_SIZE> column;
    while (ifs.getline(buf.get(), buf.size())) {
      const size_t size = tokenize(buf.get(), " ",
                                   column.get(), buf.size());
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
    std::ifstream ifs(WPATH(str_train_file.c_str()));
    CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;

    const std::string model_debug_file = std::string(model_file) + ".svm";
    std::ofstream ofs(WPATH(model_debug_file.c_str()));

    scoped_fixed_array<char, BUF_SIZE * 32> buf;
    scoped_fixed_array<char *, BUF_SIZE> column;
    while (ifs.getline(buf.get(), buf.size())) {
      if (dup.find(buf.get()) != dup.end())
        continue;
      dup.insert(buf.get());
      const size_t size = tokenize(buf.get(), " ", column.get(),
                                   buf.size());
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
    Unlink(str_train_file.c_str());
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
    std::set<int> active_feature;
    for (size_t i = 0; i < model->size(); ++i) {
      for (const int *fp = model->x(i); *fp >= 0; ++fp) {
        active_feature.insert(*fp);
      }
    }

    std::map<int, int> old2new;
    int id = 0;
    for (std::map<std::string, int>::const_iterator it = dic.begin();
         it != dic.end(); ++it) {
      if (active_feature.find(it->second) != active_feature.end()) {
        if (old2new.find(it->second) == old2new.end()) {
          old2new[it->second] = id;
          ++id;
        }
      }
    }

    std::ofstream ofs(WPATH(model_file));
    for (std::map<std::string, int>::const_iterator it = dic.begin();
         it != dic.end(); ++it) {
      if (active_feature.find(it->second) != active_feature.end()) {
        ofs << old2new[it->second] << ' ' << it->first << std::endl;
      }
    }
    ofs << std::endl;
    ofs << model->degree() << std::endl;
    ofs << model->bias() << std::endl;
    for (size_t i = 0; i < model->size(); ++i) {
      ofs << model->alpha(i);
      std::vector<int> new_x;
      for (const int *fp = model->x(i); *fp >= 0; ++fp) {
        new_x.push_back(old2new[*fp]);
      }
      std::sort(new_x.begin(), new_x.end());
      for (std::vector<int>::const_iterator it = new_x.begin();
           it != new_x.end(); ++it) {
        ofs << ' ' << *it;
      }
      ofs << std::endl;
    }
  }

  Unlink(str_train_file.c_str());
  Unlink(id_train_file.c_str());
  Unlink(svm_learn_model_file.c_str());

  std::cout << "\nDone! ";

  return true;
}
}
