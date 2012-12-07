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

inline uint64 getIndex(int index) {
  return index + 1;   // index '0' is reserved for bias term.
}

inline uint64 getIndex(int index1, int index2) {
  const uint64 result = index1 + 1;
  return static_cast<uint64>(result << 32 | index2);
}

double classify(const std::vector<int> &x,
                const std::vector<float> &w,
                const hash_map<uint64, int> &dic) {
  double result = w[0];

  for (size_t i = 0; i < x.size(); ++i) {
    const uint64 index = getIndex(x[i]);
    const hash_map<uint64, int>::const_iterator
        it = dic.find(index);
    if (it != dic.end()) {
      result += w[it->second];
    }
  }

  for (size_t i = 0; i < x.size(); ++i) {
    for (size_t j = i + 1; j < x.size(); ++j) {
      const uint64 index = getIndex(x[i], x[j]);
      const hash_map<uint64, int>::const_iterator
          it = dic.find(index);
      if (it != dic.end()) {
        result += w[it->second];
      }
    }
  }

  return result;
}

void update(const std::vector<int> &x,
            double d,
            std::vector<float> *w,
            hash_map<uint64, int> *dic) {
  (*w)[0] += d;

  for (size_t i = 0; i < x.size(); ++i) {
    const uint64 index = getIndex(x[i]);
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

  for (size_t i = 0; i < x.size(); ++i) {
    for (size_t j = i + 1; j < x.size(); ++j) {
      const uint64 index = getIndex(x[i], x[j]);
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
                     const std::vector<std::vector<int> > &x,
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

  for (size_t i = 0; i < l; ++i) {
    index[i] = i;
    int s = 0;
    for (size_t j = 0; j < x[i].size(); ++j) {
      ++s;  // x[i].value * x[i].value == 1 (always)
    }
    QD[i] = (1 + s) * (1 + s);   // 2nd polynomial kernel
  }

  // initialize primal parameters
  for (size_t i = 0; i < l; ++i) {
    if (alpha[i] > 0) {
      update(x[i], y[i] * alpha[i], &w, &dic);
    }
  }

  const size_t kMaxIteration = 5000;
  for (size_t iter = 1; iter < kMaxIteration; ++iter) {
    double PGmax_new = -kINF;
    double PGmin_new = kINF;
    std::random_shuffle(index.begin(), index.begin() + active_size);

    for (size_t s = 0; s < active_size; ++s) {
      const size_t i = index[s];
      const double margin = classify(x[i], w, dic);
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
        alpha[i] = std::min(std::max(alpha[i] - G / QD[i], 0.0), C);
        const double d = (alpha[i] - alpha_old) * y[i];
        update(x[i], d, &w, &dic);
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
SVMModel *SVMSolver::learn(const SVMModel &example,
                           const SVMModel &prev_model,
                           double cost) {
  CHECK_DIE(example.size() > 0) << "example size is 0";

  std::vector<std::vector<int> > x;
  std::vector<double> y;
  std::vector<double> alpha;

  SVMModel *model = new SVMModel;
  *(model->mutable_dic()) = example.dic();
  model->set_param("C", cost);
  model->set_param("degree", 2);
  model->set_param("bias",   0.0);

  for (size_t i = 0; i < example.size(); ++i) {
    x.push_back(example.x(i));
    y.push_back(example.alpha(i) > 0 ? +1 : -1);
    alpha.push_back(0.0);
  }

  if (prev_model.size() > 0) {
    const std::map<std::string, int> &dic = prev_model.dic();
    CHECK_DIE(!dic.empty());
    std::map<int, int> old2new;
    for (std::map<std::string, int>::const_iterator it = dic.begin();
         it != dic.end(); ++it) {
      CHECK_DIE(!it->first.empty());
      CHECK_DIE(it->second >= 0);
      const int id = model->id(it->first);
      CHECK_DIE(id != -1);
      old2new[it->second] = id;
    }

    for (size_t i = 0; i < prev_model.size(); ++i) {
      std::vector<int> tmp = prev_model.x(i);  // copy
      CHECK_DIE(!tmp.empty());
      for (size_t j = 0; j < tmp.size(); ++j) {
        tmp[j] = old2new[tmp[j]];
      }
      x.push_back(tmp);
      y.push_back(prev_model.alpha(i) > 0 ? +1 : -1);
      alpha.push_back(std::fabs(prev_model.alpha(i)));
    }
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

  model->compress();

  std::cout << "Done!\n\n";

  return model;
}
}  // CaboCha
