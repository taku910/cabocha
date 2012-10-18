// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm_learn.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>
#include "common.h"
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

#if 0
double classifyWithKernel(const int *xx,
                          const std::vector<const int *> &x,
                          const std::vector<double> &alpha,
                          const std::vector<float> &y) {
  double result = 0.0;
  for (size_t i = 0; i < alpha.size(); ++i) {
    if (alpha[i] == 0.0) {
      continue;
    }
    const int s = dot(xx, x[i]);
    result += y[i] * alpha[i] * (s + 1) * (s + 1);
  }
  return result;
}
#endif

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
}  // namespace

// static
SVMModel *SVMSolver::learn(const std::vector<double> &y_,  // label
                           const std::vector<const int *> &x_,
                           const SVMModel &prev_svm_model,
                           double C,
                           size_t degree) {
  CHECK_DIE(degree == 2)
      << "SVMSolver only supports the case of degree == 2";

  std::vector<double> y(y_);
  std::vector<const int *> x(x_);

  // dual parametes
  std::vector<double> alpha(x_.size(), 0.0);

  CHECK_DIE(y.size() == x.size());
  CHECK_DIE(y.size() == alpha.size());

  for (size_t i = 0; i < prev_svm_model.size(); ++i) {
    const double a = prev_svm_model.alpha(i);
    y.push_back(a > 0 ? +1 : -1);
    x.push_back(prev_svm_model.x(i));
    alpha.push_back(std::abs(a));
  }

  CHECK_DIE(y.size() == x.size());
  CHECK_DIE(y.size() == alpha.size());

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
        std::cout << "\nCheking all parameters..." << std::endl;
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

  SVMModel *model = new SVMModel;
  model->set_degree(degree);
  model->set_bias(0.0);

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
      model->add(alpha[i] * y[i], x[i]);
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
  std::cout << "Done! ";

  return model;
}
}  // CaboCha
