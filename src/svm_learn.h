// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm_learn.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <vector>

namespace CaboCha {

class SVMModel {
 public:
  double bias() const { return bias_; }
  void set_bias(double bias) { bias_  = bias; }
  size_t degree() const { return degree_; }
  void set_degree(size_t degree) { degree_ = degree; }
  size_t size() const { return alpha_.size(); }
  double alpha(size_t i) const { return alpha_[i]; }
  const int *x(size_t i) const { return x_[i]; }

  void add(double alpha, const int *x) {
    alpha_.push_back(alpha);
    x_.push_back(x);
  }

  double classify(const int *x) const;

  SVMModel(): bias_(0.0), degree_(0) {}

 private:
  double bias_;
  size_t degree_;
  std::vector<double> alpha_;
  std::vector<const int *> x_;
};

class SVMSolver {
 public:
  static SVMModel *learn(const std::vector<double> &y,
                         const std::vector<const int *> &x,
                         const SVMModel &prev_svm_model,
                         double C,
                         size_t degree);
};
}
