// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm.h 44 2008-02-03 14:59:21Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_SVM_H_
#define CABOCHA_SVM_H_

#include <vector>
#include <map>
#include "darts.h"
#include "mmap.h"

namespace CaboCha {

class SVMInterface {
 public:
  explicit SVMInterface() {}
  virtual ~SVMInterface() {}
  const char *what() { return what_.str(); }
  virtual bool open(const char *filename) = 0;
  virtual void close() = 0;
  virtual double classify(const std::vector<const char *> &features) const = 0;

 protected:
  whatlog what_;
};

class SVM : public SVMInterface {
 public:
  explicit SVM();
  virtual ~SVM();
  virtual bool open(const char *filename);
  virtual void close();
  virtual double classify(const std::vector<const char *> &features) const;

  bool static compile(const char *filename,
                      const char *output,
                      float sigma,
                      size_t minsup);

 private:
  double classify(const std::vector<int> &ary) const;

  unsigned int degree_;
  int bias_;
  double normalzie_factor_;
  Mmap<char> mmap_;
  Darts::DoubleArray da_;   // str -> id double array
  Darts::DoubleArray eda_;   // trie -> cost double array
};

// non-PKE:
// used only for debugging
class SVMTest: public SVMInterface {
 public:
  explicit SVMTest();
  virtual ~SVMTest();
  virtual bool open(const char *filename);
  virtual void close();
  virtual double classify(const std::vector<const char *> &features) const;

 private:
  double classify(const std::vector<int> &ary) const;

  unsigned int degree_;
  double bias_;
  std::map<std::string, int> dic_;
  std::vector<double> w_;
  std::vector<std::vector<int> > x_;
};
}
#endif
