// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm_learn.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <cstring>
#include <iostream>
#include <vector>
#include <cmath>
#include "utils.h"
#include "timer.h"
#include "svm_learn.h"


namespace CaboCha {

static const double EPS_A = 1e-12;
static const short LOWER_BOUND = -1;
static const short UPPER_BOUND = +1;
static const short FREE        = 0;

class QMatrix;

class SVMSolverImpl {
 private:

  size_t                        l_;
  int                           **x_;
  double                        *y_;
  QMatrix                       *q_matrix_;
  double                        C_;
  double                        eps_;
  size_t                        shrink_size_;
  size_t                        active_size_;
  double                        lambda_eq_;
  double                        shrink_eps_;
  size_t                        hit_old_;
  size_t                        iter_;
  size_t                        degree_;
  std::vector<double>           G_;
  std::vector<double>           alpha_;
  std::vector<short>            status_;
  std::vector<int>              active2index_;
  std::vector<unsigned short>   shrink_iter_;

  inline void swap_index(size_t i, size_t j);

  inline bool is_upper_bound(size_t i) const {
    return (status_[i] == UPPER_BOUND);
  }

  inline bool is_lower_bound(size_t i) const {
    return (status_[i] == LOWER_BOUND);
  }

  inline bool is_free(size_t i) const {
    return (status_[i] == FREE);
  }

  inline int alpha2status(const double alpha) const {
    if (alpha >= C_ - EPS_A) return UPPER_BOUND;
    else if (alpha <= EPS_A) return LOWER_BOUND;
    else                     return FREE;
  }
  void    learn_sub();
  size_t  check_inactive();

 public:
  SVMModel *learn(size_t l,
                  double *y,
                  int    **x,
                  double C,
                  size_t degree,
                  double cache_size);
};

SVMModel *SVMSolver::learn(size_t l,
                           double *y,
                           int    **x,
                           double C,
                           size_t degree,
                           double cache_size) {
  SVMSolverImpl impl;
  return impl.learn(l, y, x, C, degree, cache_size);
}

inline int dot(const int *x1, const int * x2) {
  register int sum = 0;
  while (*x1 >= 0 && *x2 >= 0) {
    if (*x1 == *x2) {
      sum++;
      ++x1;
      ++x2;
    } else if (*x1 < *x2) {
      ++x1;
    } else {
      ++x2;
    }
  }
  return sum;
}

double SVMModel::classify(const int *x) const {
  double result = -bias_;
  for (size_t i = 0; i < x_.size(); ++i) {
    result += alpha_[i] * std::pow(static_cast<float>(1 + dot(x_[i], x)),
                                   static_cast<float>(degree_));
  }
  return result;
}

template <typename T>
class Cache {
 private:
  struct head_t {
    head_t  *prev, *next;    // a cicular list
    int     index;
    T* data;
  };

  size_t   l_;
  size_t   free_size_;
  double   cache_size_;
  head_t*  lru_head_;
  head_t** index2head_;

  inline size_t get_cache_size(size_t l, double cache_size) {
    return _min(l,
                _max(static_cast<size_t>(2),
                     static_cast<size_t>
                     (1024 * 1024 * cache_size/(sizeof(T) * l))));
  }

  // delete h from current postion
  inline void delete_lru(head_t *h) {
    h->prev->next = h->next;
    h->next->prev = h->prev;
  }

  // insert h to lru - 1, position
  inline void insert_lru(head_t *h) {
    h->next = lru_head_;
    h->prev = lru_head_->prev;
    h->prev->next = h;
    h->next->prev = h;
  }

 public:
  size_t size;

  void delete_index(size_t index) {
    head_t *h = index2head_[index];
    if (h) {
      if (lru_head_ != h) {
        delete_lru(h);
        insert_lru(h);
      }
      lru_head_ = h;
    }
  }

  inline bool data(size_t index, T **data) {
    head_t *h = index2head_[index];
    if (h) {
      if (lru_head_ == h) {
        lru_head_ = lru_head_->next;
      } else {
        delete_lru(h);
        insert_lru(h);
      }
      *data = h->data;
      return true;
    } else {
      h = lru_head_;
      lru_head_ = lru_head_->next;
      if (h->index != -1)
        index2head_[h->index] = 0;
      h->index = index;
      index2head_[index] = h;
      *data = h->data;
      return false;
    }
  }

  void swap_index(size_t i, size_t j) {
    std::swap(index2head_[i], index2head_[j]);
    for (head_t *h = lru_head_;; h = h->next) {
      if (h->index == static_cast<int>(i))
        h->index = static_cast<int>(j);
      else if (h->index == static_cast<int>(j))
        h->index = static_cast<int>(i);
      std::swap(h->data[i], h->data[j]);
      if (h == lru_head_->prev) break;
    }
  }

  // change the size of each elements to be _l
  // if _l < l, allocate new erea for cache
  void update(size_t l) {
    const size_t new_size = get_cache_size(l, cache_size_);
    if (100 * new_size/size >= 110 && new_size < l) {
      // realloc
      for (head_t *h = lru_head_;;h = h->next) {
        T *new_data = new T[l];
        memcpy(new_data, h->data, sizeof(*new_data) * l);
        delete [] h->data;
        h->data = new_data;
        if (h == lru_head_->prev) break;
      }

      // new node
      for (size_t i = 0; i < (new_size - size); ++i) {
        head_t *h = new head_t;
        h->data  = new T[l];
        h->index = -1;
        insert_lru(h);
        lru_head_ = h;
      }

      size = new_size;
    }

    l_ = l;
  }

  Cache(size_t l, double cache_size):
      l_(l), cache_size_(cache_size) {
    size = get_cache_size(l, cache_size_);
    free_size_ = 0;
    head_t *start = new head_t;
    head_t *prev  = start;
    start->index = -1;
    start->data = new T[l];
    for (size_t i = 1; i < size; ++i) {
      head_t *head = new head_t;
      head->prev = prev;
      prev->next = head;
      head->index = -1;
      head->data = new T[l];
      prev = head;
    }
    prev->next = start;
    start->prev = prev;
    lru_head_ = start;
    index2head_ = new head_t * [l];
    for (size_t i = 0; i < l; ++i)
      index2head_[i] = 0;
  }

  ~Cache() {
    delete [] index2head_;
    bool flag = true;
    head_t *end = lru_head_->prev;
    head_t *h = lru_head_;

    while (flag) {
      delete [] h->data;
      head_t *tmp = h->next;
      if (h == end) flag = false;
      delete h;
      h = tmp;
    }
  }
};

class QMatrix {
 private:
  int                  **x_;
  double                *y_;
  Cache<float>          *cache_;
  Cache<unsigned char>  *cache_binary_;
  float                  binary_kernel_cache_[256];
  double                 cache_size_;
  size_t                 size_;
  size_t                 hit_;
  size_t                 miss_;

 public:
  size_t size() const { return size_; }
  size_t hit()  const { return hit_;  }
  size_t miss() const { return miss_; }
  void set(double *y, int **x) {
    y_ = y;
    x_ = x;
  }

  explicit QMatrix(size_t l, double cache_size, size_t degree) {
    cache_size_ = cache_size;
    cache_ = new Cache<float>(l, cache_size_ * 0.3);
    cache_binary_ = new Cache<unsigned char>(l, cache_size_ * 0.7);
    size_ = cache_->size + cache_binary_->size;
    for (size_t i = 0; i < 256; ++i) {
      binary_kernel_cache_[i] = std::pow(static_cast<float>(i + 1),
                                         static_cast<float>(degree));
    }
    hit_ = miss_ = 0;
  }

  virtual ~QMatrix() {
    delete cache_;
    delete cache_binary_;
  }

  void rebuild(size_t active_size) {
    delete cache_;
    delete cache_binary_;
    cache_ = new Cache<float>(active_size, cache_size_ * 0.3);
    cache_binary_ = new Cache<unsigned char>(active_size, cache_size_ * 0.7);
    size_ = cache_->size + cache_binary_->size;
  }

  void swap_index(size_t i, size_t j) {
    cache_->swap_index(i, j);
    cache_->delete_index(j);
    cache_binary_->swap_index(i, j);
    cache_binary_->delete_index(j);
  }

  void update(size_t active_size) {
    size_ = 0;
    cache_->update(active_size);
    cache_binary_->update(active_size);
    size_ += cache_->size;
    size_ += cache_binary_->size;
  }

  void delete_index(const size_t i) {
    cache_->delete_index(i);
    cache_binary_->delete_index(i);
  }

  float *Q(size_t i, size_t active_size) {
    float *data = 0;
    if (cache_->data(i, &data)) {
      hit_++;
    } else {
      unsigned char *data2;
      if (cache_binary_->data(i, &data2)) {
        for (size_t j = 0; j < active_size; j++)
          data[j] = y_[i] * y_[j] * binary_kernel_cache_[data2[j]];
        hit_++;
      } else {
        for (size_t j = 0; j < active_size; j++) {
          data2[j] = static_cast<unsigned char>(dot(x_[i], x_[j]));
          data[j] = y_[i] * y_[j] * binary_kernel_cache_[data2[j]];
        }
        miss_++;
      }
    }
    return data;
  }
};

SVMModel *SVMSolverImpl::learn(size_t l,
                               double *y,
                               int   **x,
                               double C,
                               size_t degree,
                               double cache_size) {
  progress_timer timer;
  l_            = l;
  C_            = C;
  eps_          = 0.001;
  shrink_size_  = 100;
  shrink_eps_   = 2.0;
  active_size_  = l;
  iter_         = 0;
  hit_old_      = 0;
  y_            = y;
  x_            = x;
  degree_       = degree;

  alpha_.resize(l_);
  G_.resize(l_);
  shrink_iter_.resize(l_);
  status_.resize(l_);
  active2index_.resize(l_);

  std::fill(G_.begin(), G_.end(), -1.0);
  std::fill(alpha_.begin(), alpha_.end(), 0.0);
  std::fill(shrink_iter_.begin(), shrink_iter_.end(), 0);

  for (size_t i = 0; i < l_; ++i) {
    status_[i] = alpha2status(alpha_[i]);
    active2index_[i] = i;
  }

  q_matrix_ = new QMatrix(l_, cache_size, degree);
  q_matrix_->set(y_, x_);

  for (;;) {
    learn_sub();
    if (check_inactive() == 0)  break;
    q_matrix_->rebuild(active_size_);
    q_matrix_->set(y, x);
    shrink_eps_ = 2.0;
  }

  // calculate threshold b
  const double rho = lambda_eq_;

  // make output model
  SVMModel *out_model = new SVMModel;
  out_model->set_bias(-rho);
  out_model->set_degree(degree_);

  size_t err = 0;
  size_t bsv = 0;
  size_t sv = 0;
  double loss = 0.0;
  for (size_t i = 0; i < l_; ++i) {
    const double d = G_[i] + y_[i] * rho + 1.0;
    if (d < 0) ++err;
    if (d < (1 - eps_)) loss += (1 - d);
    if (alpha_[i] >= C_ - EPS_A) ++bsv;  // upper bound
    if (alpha_[i] > EPS_A) {
      out_model->add(alpha_[i] * y[i], x[i]);
      ++sv;
    }
  }

  delete q_matrix_;

  std::cout << "Error: " << err << std::endl;
  std::cout << "L1 Loss: " <<  loss << std::endl;
  std::cout << "BSV: " << bsv << std::endl;
  std::cout << "SV: " << sv << std::endl;
  std::cout << "Done! ";

  return out_model;
}

// swap i and j
inline void SVMSolverImpl::swap_index(size_t i, size_t j) {
  std::swap(y_[i],            y_[j]);
  std::swap(x_[i],            x_[j]);
  std::swap(alpha_[i],        alpha_[j]);
  std::swap(status_[i],       status_[j]);
  std::swap(G_[i],            G_[j]);
  std::swap(shrink_iter_[i],  shrink_iter_[j]);
  std::swap(active2index_[i], active2index_[j]);
}

void SVMSolverImpl::learn_sub() {
  while (++iter_) {
    /////////////////////////////////////////////////////////
    // Select Working set
    static const double kINF = 1e+37;
    double Gmax1 = -kINF;
    int i = -1;
    double Gmax2 = -kINF;
    int j = -1;

    for (size_t k = 0; k < active_size_; ++k) {
      if (y_[k] > 0) {
        if (!is_upper_bound(k)) {
          if (-G_[k] > Gmax1) {
            Gmax1 = -G_[k];
            i = k;
          }
        }

        if (!is_lower_bound(k)) {
          if (G_[k] > Gmax2) {
            Gmax2 = G_[k];
            j = k;
          }
        }
      } else {
        if (!is_upper_bound(k)) {
          if (-G_[k] > Gmax2) {
            Gmax2 = -G_[k];
            j = k;
          }
        }

        if (!is_lower_bound(k)) {
          if (G_[k] > Gmax1) {
            Gmax1 = G_[k];
            i = k;
          }
        }
      }
    }

    /////////////////////////////////////////////////////////
    //
    // Solving QP sub problems
    //
    const double old_alpha_i = alpha_[i];
    const double old_alpha_j = alpha_[j];

    const float *Q_i = q_matrix_->Q(i, active_size_);
    const float *Q_j = q_matrix_->Q(j, active_size_);

    if (y_[i] * y_[j] < 0) {
      const double L = _max(0.0, alpha_[j] - alpha_[i]);
      const double H = _min(C_, C_ + alpha_[j] - alpha_[i]);
      alpha_[j] += (-G_[i] - G_[j]) /(Q_i[i] + Q_j[j] + 2 * Q_i[j]);
      if (alpha_[j] >= H)      alpha_[j] = H;
      else if (alpha_[j] <= L) alpha_[j] = L;
      alpha_[i] += (alpha_[j] - old_alpha_j);
    } else {
      const double L = _max(0.0, alpha_[i] + alpha_[j] - C_);
      const double H = _min(C_, alpha_[i] + alpha_[j]);
      alpha_[j] += (G_[i] - G_[j]) /(Q_i[i] + Q_j[j] - 2 * Q_i[j]);
      if (alpha_[j] >= H)      alpha_[j] = H;
      else if (alpha_[j] <= L) alpha_[j] = L;
      alpha_[i] -= (alpha_[j] - old_alpha_j);
    }

    /////////////////////////////////////////////////////////
    //
    // update status
    //
    status_[i] = alpha2status(alpha_[i]);
    status_[j] = alpha2status(alpha_[j]);

    const double delta_alpha_i = alpha_[i] - old_alpha_i;
    const double delta_alpha_j = alpha_[j] - old_alpha_j;

    /////////////////////////////////////////////////////////
    //
    // Update O and Calculate \lambda^eq for shrinking, Calculate lambda^eq,
    // (c.f. Advances in Kernel Method pp.175)
    // lambda_eq_ = 1/|FREE| \sum_{i \in FREE} y_i -
    //              \sum_{l} y_i \alpha_i k(x_i,x_j)(11.29)
    //
    lambda_eq_ = 0.0;
    size_t size_A = 0;
    for (size_t k = 0; k < active_size_; ++k) {
      G_[k] += Q_i[k] * delta_alpha_i + Q_j[k] * delta_alpha_j;
      if (is_free(k)) {
        lambda_eq_ -= G_[k] * y_[k];
        ++size_A;
      }
    }

    /////////////////////////////////////////////////////////
    //
    // Select example for shrinking,
    // (c.f. 11.5 Efficient Implementation in Advances in
    // Kernel Method pp. 175)
    //
    lambda_eq_ = size_A ? (lambda_eq_ / size_A) : 0.0;
    double kkt_violation = 0.0;

    for (size_t k = 0; k < active_size_; ++k) {
      const double lambda_up = -(G_[k] + y_[k] * lambda_eq_);
      // lambda_lo = -lambda_up

      // termination criteria(11.32,11.33,11.34)
      if (!is_lower_bound(k) && lambda_up < -kkt_violation)
        kkt_violation = -lambda_up;

      if (!is_upper_bound(k) && lambda_up >  kkt_violation)
        kkt_violation =  lambda_up;

      // "If the estimate(11.30) or(11.31) was positive
      // (or above some threshold) at each of the last h iterations,
      // it is likely that this will be true at the  optimal solution"
      // lambda^up(11.30) lambda^low = lambda^up * status_[k]
      if (lambda_up * status_[k] > shrink_eps_) {
        if (shrink_iter_[k]++ > shrink_size_) {
          --active_size_;
          swap_index(k, active_size_);  // remove this data from working set
          q_matrix_->swap_index(k, active_size_);
          q_matrix_->update(active_size_);
          --k;
        }
      } else {
        // reset iter, if current data dose not satisfy
        // the condition (11.30),(11.31)
        shrink_iter_[k] = 0;
      }
    }

    /////////////////////////////////////////////////////////
    //
    // Check terminal criteria, show information of iteration
    //
    if (kkt_violation < eps_) break;
    if ((iter_ % 50) == 0) std::cout << "." << std::flush;
    if ((iter_ % 1000) == 0) {
      std::cout << ' ' << iter_ << ' '
                << active_size_ << ' '
                << q_matrix_->size() << ' '
                << kkt_violation << ' '
                << 100.0 * (q_matrix_->hit() - hit_old_)/2000 << "% "
                << 100.0 * q_matrix_->hit()/(q_matrix_->hit()
                                             + q_matrix_->miss()) << "%"
                << std::endl;
      hit_old_ = q_matrix_->hit();
      shrink_eps_ = shrink_eps_ * 0.7 + kkt_violation * 0.3;
    }
  }
}

size_t SVMSolverImpl::check_inactive() {
  // final check
  std::cout << std::endl;

  // make dummy classifier
  SVMModel tmp_model;
  tmp_model.set_bias(-lambda_eq_);
  tmp_model.set_degree(degree_);
  for (size_t i = 0; i < l_; ++i) {
    if (!is_lower_bound(i))
      tmp_model.add(alpha_[i] * y_[i], x_[i]);
  }

  size_t react_num = 0;
  for (int k = l_ - 1; k >= static_cast<int>(active_size_); --k) {
    const double lambda_up = 1 - y_[k] * tmp_model.classify(x_[k]);
    G_[k] = y_[k] * tmp_model.bias() - lambda_up;

    // Oops!, must be added to the active example.
    if ((!is_lower_bound(k) && lambda_up < -eps_) ||
        (!is_upper_bound(k) && lambda_up >  eps_)) {
      swap_index(k, active_size_);
      ++active_size_;
      ++k;
      ++react_num;
    }
  }

  std::cout <<  "re-activated: " << react_num << std::endl;
  return react_num;
}
}
