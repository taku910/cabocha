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
                               double cost) {
  CHECK_DIE(cost > 0.0) << "cost must be positive value";
  CHECK_DIE(degree == 2) << "degree must be degree==2";

  std::map<std::string, int> dic;
  const std::string str_train_file = std::string(model_file) + ".str";
  const std::string id_train_file = std::string(model_file) + ".id";
  const std::string svm_learn_model_file = std::string(model_file) + ".model";
  const std::string model_debug_file = std::string(model_file) + ".svm";

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

  // read training data and make dic
  {
    int id = 1;
    std::ifstream ifs(WPATH(str_train_file.c_str()));
    CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;
    scoped_fixed_array<char, BUF_SIZE * 32> buf;
    scoped_fixed_array<char *, BUF_SIZE> column;
    while (ifs.getline(buf.get(), buf.size())) {
      const size_t size = tokenize(buf.get(), " ",
                                   column.get(), column.size());
      CHECK_DIE(size >= 2);
      for (size_t i = 1; i < size; ++i) {
        const std::string key = column[i];
        if (dic.find(key) == dic.end()) {
          dic[key] = id;
          ++id;
        }
      }
    }
  }

  std::vector<const int *> x;
  std::vector<double> y;
  FreeList<int> feature_freelist(8192);

  {
    std::ifstream ifs(WPATH(str_train_file.c_str()));
    CHECK_DIE(ifs) << "no such file or directory: " << str_train_file;

    std::ofstream ofs(WPATH(model_debug_file.c_str()));

    scoped_fixed_array<char, BUF_SIZE * 32> buf;
    scoped_fixed_array<char *, BUF_SIZE> column;
    while (ifs.getline(buf.get(), buf.size())) {
      const size_t size = tokenize(buf.get(), " ", column.get(),
                                   column.size());
      CHECK_DIE(size >= 2);
      std::vector<int> tmp;
      for (size_t i = 1; i < size; ++i) {
        const std::string key = column[i];
        CHECK_DIE(dic.find(key) != dic.end());
        tmp.push_back(dic[key]);
      }
      std::sort(tmp.begin(), tmp.end());

      const float label = std::atof(column[0]);
      ofs << label;
      for (size_t i = 0; i < tmp.size(); ++i) {
        CHECK_DIE(tmp[i] > 0);
        ofs << " " << tmp[i] << ":1";
      }
      ofs << std::endl;

      tmp.push_back(-1);
      int *fp = feature_freelist.alloc(tmp.size());
      std::copy(tmp.begin(), tmp.end(), fp);
      x.push_back(fp);
      y.push_back(label);
    }
  }

  CHECK_DIE(x.size() >= 2) << "training data is too small";
  CHECK_DIE(x.size() == y.size());

  scoped_ptr<SVMModel> model;
  model.reset(SVMSolver::learn(y, x, cost, degree));
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
        CHECK_DIE(old2new.find(it->second) != old2new.end());
        ofs << old2new[it->second] << ' ' << it->first << std::endl;
      }
    }

    ofs.setf(std::ios::fixed, std::ios::floatfield);
    ofs.precision(16);

    ofs << std::endl;
    ofs << model->degree() << std::endl;
    ofs << model->bias() << std::endl;
    for (size_t i = 0; i < model->size(); ++i) {
      ofs << model->alpha(i);
      std::vector<int> new_x;
      for (const int *fp = model->x(i); *fp >= 0; ++fp) {
        CHECK_DIE(old2new.find(*fp) != old2new.end());
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

  Unlink(model_debug_file.c_str());
  Unlink(str_train_file.c_str());
  Unlink(id_train_file.c_str());
  Unlink(svm_learn_model_file.c_str());

  std::cout << "\nDone! ";

  return true;
}
}
