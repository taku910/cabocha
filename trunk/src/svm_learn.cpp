// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm_learn.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include "common.h"
#include "scoped_ptr.h"
#include "svm.h"
#include "svm_learn.h"
#include "timer.h"
#include "utils.h"

namespace CaboCha {
namespace {
const double kEPS = 0.1;
const double kINF = 1e+37;

inline uint64 getIndex(int index, size_t size) {
  return index + 1;   // index '0' is reserved for bias term.
}

inline uint64 getIndex(int index1, int index2, size_t size) {
  return (index1 + 1) * (size + 1) + index2 + 1;
}

double classify(const int *x, int max_index,
                const std::vector<float> &w,
                const hash_map<uint64, int> &dic) {
  double result = w[0];

  for (size_t i = 0; x[i] >= 0; ++i) {
    const uint64 index = getIndex(x[i], max_index + 1);
    const hash_map<uint64, int>::const_iterator
        it = dic.find(index);
    if (it != dic.end()) {
      result += w[it->second];
    }
  }

  for (size_t i = 0; x[i] >= 0; ++i) {
    for (size_t j = i + 1; x[j] >= 0; ++j) {
      const uint64 index = getIndex(x[i], x[j], max_index + 1);
      const hash_map<uint64, int>::const_iterator
          it = dic.find(index);
      if (it != dic.end()) {
        result += w[it->second];
      }
    }
  }

  return result;
}

void update(const int *x, int max_index, double d,
            std::vector<float> *w,
            hash_map<uint64, int> *dic) {
  (*w)[0] += d;

  for (size_t i = 0; x[i] >= 0; ++i) {
    const uint64 index = getIndex(x[i], max_index + 1);
    int n = dic->size();
    std::pair<hash_map<uint64, int>::iterator, bool>
        r = dic->insert(std::make_pair(index, n));
    if (r.second) {  // could insert (new index)
      w->resize(n + 1);
      (*w)[n] = 0.0;
    } else {
      n = r.first->second;
    }
    (*w)[n] += 3 * d;
  }

  for (size_t i = 0; x[i] >= 0; ++i) {
    for (size_t j = i + 1; x[j] >= 0; ++j) {
      const uint64 index = getIndex(x[i], x[j], max_index + 1);
      int n = dic->size();
      std::pair<hash_map<uint64, int>::iterator, bool>
          r = dic->insert(std::make_pair(index, n));
      if (r.second) {  // could insert (new index)
        w->resize(n + 1);
        (*w)[n] = 0.0;
      } else {
        n = r.first->second;
      }
      (*w)[n] += 2 * d;
    }
  }
}

bool solveParameters(const std::vector<double> &y,
                     const std::vector<const int *> &x,
                     double C,
                     std::vector<double> *alpha_) {
  std::vector<double> alpha(*alpha_);

  CHECK_DIE(alpha.size() == x.size());
  CHECK_DIE(y.size() == x.size());

  const size_t l = y.size();
  size_t active_size = l;
  double PGmax_old = kINF;
  double PGmin_old = -kINF;
  std::vector<size_t> index(l, 0);
  std::vector<double> QD(l, 0.0);     // kernel(x_i * x_i)
  std::vector<double> GA(l, 0.0);     // margin + 1

  std::vector<float> w(1, 0.0);   // primal parameters
  hash_map<uint64, int> dic;

  // index for the bias.
  dic[0] = 0;

  int max_index = 0;

  for (size_t i = 0; i < l; ++i) {
    index[i] = i;
    int s = 0;
    for (size_t j = 0; x[i][j] >= 0; ++j) {
      max_index = std::max(x[i][j], max_index);
      ++s;  // x[i].value * x[i].value == 1 (always)
    }
    QD[i] = (1 + s) * (1 + s);   // 2nd polynomial kernel
  }

  // initialize primal parameters
  for (size_t i = 0; i < l; ++i) {
    if (alpha[i] > 0) {
      update(x[i], max_index, y[i] * alpha[i], &w, &dic);
    }
  }

  const size_t kMaxIteration = 5000;
  for (size_t iter = 1; iter < kMaxIteration; ++iter) {
    double PGmax_new = -kINF;
    double PGmin_new = kINF;
    std::random_shuffle(index.begin(), index.begin() + active_size);

    for (size_t s = 0; s < active_size; ++s) {
      const size_t i = index[s];
      const double margin = classify(x[i], max_index, w, dic);
      GA[i] = margin * y[i] - 1;
      const double G = GA[i];
      double PG = 0.0;

      if (alpha[i] == 0.0) {
        if (G > PGmax_old) {
          active_size--;
          std::swap(index[s], index[active_size]);
          s--;
          continue;
        } else if (G < 0.0) {
          PG = G;
        }
      } else if (alpha[i] == C) {
        if (G < PGmin_old) {
          active_size--;
          std::swap(index[s], index[active_size]);
          s--;
          continue;
        } else if (G > 0.0) {
          PG = G;
        }
      } else {
        PG = G;
      }

      PGmax_new = std::max(PGmax_new, PG);
      PGmin_new = std::min(PGmin_new, PG);

      if (std::fabs(PG) > 1.0e-12) {
        const double alpha_old = alpha[i];
        alpha[i] =std::min(std::max(alpha[i] - G / QD[i], 0.0), C);
        const double d = (alpha[i] - alpha_old) * y[i];
        update(x[i], max_index, d, &w, &dic);
      }
    }

    std::cout << "iter=" << iter
              << " kkt=" << PGmax_new - PGmin_new
              << " feature_size=" << w.size()
              << " active_size=" << active_size << std::endl;

    if ((PGmax_new - PGmin_new) <= kEPS) {
      if (active_size == l) {
        break;
      } else {
        std::cout << "\nChecking all parameters..." << std::endl;
        active_size = l;  // restore again
        PGmax_old = kINF;
        PGmin_old = -kINF;
        continue;
      }
    }

    PGmax_old = PGmax_new;
    PGmin_old = PGmin_new;
    if (PGmax_old <= 0) {
      PGmax_old = kINF;
    }
    if (PGmin_old >= 0) {
      PGmin_old = -kINF;
    }
  }

  size_t err = 0;
  size_t bsv = 0;
  size_t sv = 0;
  double loss = 0.0;
  double obj = 0.0;

  for (size_t i = 0; i < y.size(); ++i) {
    obj += alpha[i] * (GA[i] - 1);
    const double d = GA[i] + 1.0;
    if (d < 0) {
      ++err;
    }
    if (d < 1.0) {  // inside a margin
      loss += (1 - d);
    }
    if (alpha[i] == C) {
      ++bsv;
    }
    if (alpha[i] > 0.0) {
      ++sv;
    }
  }
  obj /= 2.0;

  std::cout << std::endl;
  std::cout << "Error: " << err << std::endl;
  std::cout << "L1 Loss: " <<  loss << std::endl;
  std::cout << "BSV: " << bsv << std::endl;
  std::cout << "SV: " << sv << std::endl;
  std::cout << "obj: " << obj << std::endl;

  // copy results.
  *alpha_ = alpha;

  return true;
}
}  // namespace

// static
SVMModel *SVMSolver::learn(const char *training_file,
                           const char *prev_model_file,
                           double cost) {
  SVMModel prev_model;
  SVMModel *model = new SVMModel;

  scoped_fixed_array<char, BUF_SIZE * 32> buf;
  scoped_fixed_array<char *, BUF_SIZE> column;

  if (prev_model_file) {
    CHECK_DIE(prev_model.open(prev_model_file))
        << "no such file or directory: " << prev_model_file;
  }

  // copy old dictionary
  std::map<std::string, int> *dic = model->mutable_dic();
  *dic = prev_model.dic();

  // create feature_string => id maping
  {
    int id = dic->size();
    std::ifstream ifs(WPATH(training_file));
    CHECK_DIE(ifs) << "no such file or directory: " << training_file;
    while (ifs.getline(buf.get(), buf.size())) {
      const size_t size = tokenize(buf.get(), " ",
                                   column.get(), column.size());
      CHECK_DIE(size >= 2);
      for (size_t i = 1; i < size; ++i) {
        const std::string key = column[i];
        if (dic->find(key) == dic->end()) {
          (*dic)[key] = id;
          ++id;
        }
      }
    }
  }

  // make training data
  {
    std::vector<const int *> x;
    std::vector<double> y;
    std::vector<double> alpha;
    std::ifstream ifs(WPATH(training_file));
    CHECK_DIE(ifs) << "no such file or directory: " << training_file;

    // load training data
    while (ifs.getline(buf.get(), buf.size())) {
      const size_t size = tokenize(buf.get(), " ", column.get(),
                                   column.size());
      CHECK_DIE(size >= 2);
      int *fp = model->alloc(size);
      for (size_t i = 1; i < size; ++i) {
        const std::string key = column[i];
        CHECK_DIE(dic->find(key) != dic->end());
        fp[i - 1] = (*dic)[key];
      }
      std::sort(fp, fp + size - 1);
      fp[size - 1] = -1;

      x.push_back(fp);
      y.push_back(std::atof(column[0]));
      alpha.push_back(0.0);
    }

    // add prev model
    for (size_t i = 0; i < prev_model.size(); ++i) {
      x.push_back(prev_model.x(i));
      y.push_back(prev_model.alpha(i) > 0 ? +1 : -1);
      alpha.push_back(std::fabs(prev_model.alpha(i)));
    }

    CHECK_DIE(x.size() >= 2) << "training data is too small";
    CHECK_DIE(x.size() == y.size());
    CHECK_DIE(alpha.size() == y.size());

    CHECK_DIE(solveParameters(y, x, cost, &alpha));

    for (size_t i = 0; i < alpha.size(); ++i) {
      if (alpha[i] > 0.0) {
        model->add(y[i] * alpha[i], x[i]);
      }
    }

    model->set_param("C", cost);
    model->set_param("degree", 2);
    model->set_param("bias",   0.0);
    model->compress();

    std::cout << "Done!\n\n";
  }

  return model;
}
}  // CaboCha
