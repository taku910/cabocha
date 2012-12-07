// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm.h 44 2008-02-03 14:59:21Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_SVM_H_
#define CABOCHA_SVM_H_

#include <map>
#include <vector>
#include <string>
#include "darts.h"
#include "freelist.h"
#include "mmap.h"

namespace CaboCha {

class Iconv;

class SVMModelInterface {
 public:
  explicit SVMModelInterface() {}
  virtual ~SVMModelInterface() {}
  const char *what() { return what_.str(); }

  void set_param(const char *key, const char *value);
  void set_param(const char *key, int value);
  void set_param(const char *key, double value);
  const char *get_param(const char *key) const;
  const std::map<std::string, std::string> &param() const { return param_; }
  std::map<std::string, std::string> *mutable_param() { return &param_; }

  virtual bool open(const char *filename) = 0;
  virtual bool save(const char *filename) const = 0;
  virtual bool compress() = 0;
  virtual void close() = 0;
  virtual int id(const std::string &key) const = 0;
  virtual double classify(const std::vector<int> &x) const = 0;

  // make const method as this is used in parser.
  virtual void add(double alpha, const std::vector<int> &x) const = 0;

  virtual double classify(const std::vector<std::string> &features) const;

 protected:
  whatlog what_;
  std::map<std::string, std::string> param_;
};

class SVMModel : public SVMModelInterface {
 public:
  size_t size() const { return alpha_.size(); }
  double alpha(size_t i) const { return alpha_[i]; }
  double y(size_t i) const { return alpha_[i] > 0 ? +1 : -1; }
  const std::vector<int> &x(size_t i) const { return x_[i]; }
  const std::map<std::string, int> &dic() const { return dic_; }
  std::map<std::string, int> *mutable_dic() { return &dic_; }

  virtual bool open(const char *filename);
  virtual void close();
  virtual int id(const std::string &key) const;
  virtual double classify(const std::vector<int> &x) const;
  virtual void add(double alpha, const std::vector<int> &x) const {
    alpha_.push_back(alpha);
    x_.push_back(x);
  }

  virtual bool save(const char *filename) const;
  virtual bool compress();

  SVMModel();
  virtual ~SVMModel();

 protected:
  mutable std::vector<double> alpha_;
  mutable std::vector<std::vector<int> > x_;
  mutable std::map<std::string, int> dic_;
};

class FastSVMModel : public SVMModelInterface {
 public:
  FastSVMModel();
  virtual ~FastSVMModel();
  virtual bool open(const char *filename);
  virtual void close();
  virtual int id(const std::string &key) const;
  virtual double classify(const std::vector<int> &x) const;

  virtual void add(double alpha, const std::vector<int> &x) const {
    return;
  }
  virtual bool save(const char *filename) const { return false; }
  virtual bool compress() { return false; }

  static bool compile(const char *filename,
                      const char *output,
                      double sigma,
                      size_t minsup,
                      Iconv *iconv);

 private:
  unsigned int degree_;
  int bias_;
  double normalzie_factor_;
  Mmap<char> mmap_;
  Darts::DoubleArray da_;   // str -> id double array
  Darts::DoubleArray eda_;   // trie -> cost double array
};
}
#endif
