// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <algorithm>
#include <functional>
#include <cmath>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include "timer.h"
#include "common.h"
#include "mmap.h"
#include "darts.h"
#include "svm.h"
#include "scoped_ptr.h"
#include "utils.h"
#include "freelist.h"

namespace CaboCha {

inline double kernel(const std::vector<int> &x1,
                     const std::vector<int> &x2,
                     unsigned int degree) {
  size_t i1 = 0;
  size_t i2 = 0;
  int n = 0;
  while (i1 < x1.size() && i2 < x2.size()) {
    if (x1[i1] == x2[i2]) {
      ++n;
      ++i1;
      ++i2;
    } else if (x1[i1] > x2[i2]) {
      ++i2;
    } else if (x1[i1] < x2[i2]) {
      ++i1;
    }
  }

  if (n == 0) {
    return 1;
  }

  ++n;
  for (unsigned int i = 1; i < degree; ++i) {
    n *= n;
  }
  return n;
}

int progress_bar_dic(size_t current, size_t total) {
  return progress_bar("emitting dic    ", current, total);
}

int progress_bar_trie(size_t current, size_t total) {
  return progress_bar("emitting trie   ", current, total);
}

void inline encodeBER(unsigned int value,
                      unsigned char *output, unsigned int *length) {
  *length = 0;
  output[(*length)++] = (value & 0x7f);
  while (value >>= 7) {
    output[*length - 1] |= 0x80;
    output[(*length)++] = (value & 0x7f);
  }
}

void inline encodeBERArray(unsigned int *begin,
                           unsigned int *end,
                           unsigned char *output,
                           unsigned int *output_len) {
  unsigned char *str = output;
  for (unsigned int *p = begin; p < end; ++p) {
    unsigned int n = 0;
    encodeBER(*p, str, &n);
    str += n;
  }
  *output_len = static_cast<unsigned int>(str - output);
}

static const unsigned int DictionaryMagicID = 0xef522177u;

static const int WEIGHT[4][5] = {
  {0, 0, 0, 0, 0},   // 0
  {1, 1, 0, 0, 0},   // 1
  {1, 3, 2, 0, 0},   // 2
  {1, 7, 12, 6, 0}   // 3
};

static const int MAX_WEIGHT[5] = { 0, 1, 3, 12 };

static const int kPKEBase = 0xfffff;  // 1048575

class PKEMine {
 public:
  PKEMine():
      transaction_(0), w_(0), out_rules_(0),
      char_freelist_(8192 * 8192), rule_freelist_(8192 * 1024),
      sigma_pos_(0.0), sigma_neg_(0.0),
      minsup_(0), degree_(0) {}

  struct Rule {
    unsigned char *f;  // feature
    unsigned int len;  // length
    float w;
  };

  bool run(const std::vector<std::vector<unsigned int> > &transaction,
           const std::vector<float> &w,  // weight
           float sigma,
           size_t minsup,
           size_t degree,
           std::vector<Rule *> *rules) {
    transaction_ = &transaction;
    w_ = &w;
    out_rules_ = rules;
    minsup_ = minsup;
    degree_ = degree;
    char_freelist_.free();
    rule_freelist_.free();

    CHECK_DIE(w.size() == transaction.size())
        << "w.size() != transaction.size()";
    CHECK_DIE(out_rules_) << "rules is NULL";
    CHECK_DIE(minsup_ > 0) << "minsup should not be 0";
    CHECK_DIE(sigma > 0.0) << "sigma should not be 0";
    CHECK_DIE(degree_ >= 1 && degree_ <= 3)
        << "degree should be 1<=degree<=3";
    CHECK_DIE(w_);
    CHECK_DIE(transaction_);

    std::vector<std::pair<size_t, int> > root;
    size_t pos_num = 0;
    size_t neg_num = 0;
    for (size_t i = 0; i < transaction_->size(); i++) {
      root.push_back(std::make_pair(i, -1));
      if (w[i] > 0) {
        ++pos_num;
      } else {
        ++neg_num;
      }
    }

    sigma_pos_ =  1.0 * sigma * pos_num / (pos_num + neg_num);
    sigma_neg_ = -1.0 * sigma * neg_num / (pos_num + neg_num);

    CHECK_DIE(sigma_pos_ != 0.0);
    CHECK_DIE(sigma_neg_ != 0.0);

    pattern_.clear();
    project(root, true);

    return true;
  }

 private:
  const std::vector<std::vector<unsigned int> >  *transaction_;
  const std::vector<float> *w_;  // weight
  std::vector<Rule *> *out_rules_;
  std::vector<unsigned int> pattern_;
  FreeList<unsigned char> char_freelist_;
  FreeList<Rule> rule_freelist_;
  float sigma_pos_;
  float sigma_neg_;
  unsigned int minsup_;
  unsigned int degree_;

  bool prune(const std::vector<std::pair<size_t, int> > &projected) {
    const size_t sup = projected.size();
    if (sup < minsup_) {
      return true;
    }

    float mu_pos = 0.0;
    float mu_neg = 0.0;
    float w = 0.0;
    for (size_t i = 0; i < projected.size(); ++i) {
      w += WEIGHT[degree_][pattern_.size()] * (*w_)[projected[i].first];
      if ((*w_)[projected[i].first] > 0)
        mu_pos += MAX_WEIGHT[degree_] * (*w_)[projected[i].first];
      else
        mu_neg += MAX_WEIGHT[degree_] * (*w_)[projected[i].first];
    }

    // output vector
    if (w <= sigma_neg_ || w >= sigma_pos_) {
      Rule *rule = rule_freelist_.alloc(1);
      rule->w = w;
      unsigned char tmp[128];
      encodeBERArray(&pattern_[0], &pattern_[0] + pattern_.size(),
                     tmp, &(rule->len));
      rule->f = char_freelist_.alloc(rule->len);
      std::copy(tmp, tmp + rule->len, rule->f);
      out_rules_->push_back(rule);
    }

    if (mu_pos < sigma_pos_ && mu_neg > sigma_neg_) {
      return true;
    }

    return false;
  }

  void project(const std::vector<std::pair<size_t, int> > &projected,
               bool is_root) {
    if (pattern_.size() >= degree_ || projected.empty())
      return;

    std::map<unsigned int, std::vector<std::pair<size_t, int> > > counter;
    for (size_t i = 0; i < projected.size(); ++i) {
      const size_t id   = projected[i].first;
      const int    pos  = projected[i].second;
      const size_t size = (*transaction_)[id].size();
      for (size_t j = pos + 1; j < size; ++j)
        counter[(*transaction_)[id][j]].push_back
            (std::make_pair(id, j));
    }

    const size_t root_size = counter.size();
    size_t processed = 0;

    for (std::map<
             unsigned int, std::vector<std::pair<size_t, int> > >
             ::const_iterator l = counter.begin();
         l != counter.end(); ++l) {
      if (is_root) {
        progress_bar("mining features ", processed+1, root_size);
        ++processed;
      }
      pattern_.push_back(l->first);
      if (!prune(l->second)) {
        project(l->second, false);
      }
      pattern_.resize(pattern_.size() - 1);
    }
  }
};

SVM::SVM(): degree_(3), bias_(0), normalzie_factor_(1.0) {}
SVM::~SVM() {}

void SVM::close() {
  mmap_.close();
}

bool SVM::open(const char *filename) {
  CHECK_FALSE(mmap_.open(filename)) <<  mmap_.what();
  const char *ptr = mmap_.begin();

  unsigned int magic = 0;
  read_static<unsigned int>(&ptr, magic);
  CHECK_FALSE((magic ^ DictionaryMagicID) == mmap_.size())
      << "dictionary file is broken: " << filename;

  unsigned int version = 0;
  read_static<unsigned int>(&ptr, version);
  CHECK_FALSE(version == MODEL_VERSION)
      << "incompatible version: " << version;

  // model parameters
  unsigned int dsize = 0;
  unsigned int tsize = 0;
  read_static<unsigned int>(&ptr, dsize);  // double array
  read_static<unsigned int>(&ptr, tsize);  // trie
  CHECK_FALSE(dsize != 0);
  CHECK_FALSE(tsize != 0);

  read_static<unsigned int>(&ptr, degree_);
  CHECK_FALSE(degree_ >= 1 && degree_ <= 3);

  read_static<int>(&ptr, bias_);
  read_static<double>(&ptr, normalzie_factor_);
  CHECK_FALSE(normalzie_factor_ > 0.0);

  da_.set_array(reinterpret_cast<void *>(const_cast<char *>(ptr)));
  ptr += dsize;
  eda_.set_array(reinterpret_cast<void *>(const_cast<char *>(ptr)));
  ptr += tsize;

  CHECK_FALSE(ptr == mmap_.end())
      << "dictionary file is broken: " << filename;

  dot_buf_.reserve(8192);

  return true;
}

double SVM::classify(size_t argc, char **argv) {
  dot_buf_.clear();
  for (size_t i = 0; i < argc; ++i) {
    const int r =
        da_.exactMatchSearch<Darts::DoubleArray::result_type>(argv[i]);
    if (r != -1) {
      dot_buf_.push_back(r);
    }
  }
  std::sort(dot_buf_.begin(), dot_buf_.end());
  return classify(dot_buf_);
}

double SVM::classify(const std::vector<int> &ary) {
  const size_t size = ary.size();
  size_t p = 0;
  int r = 0;
  int score = -bias_;

  unsigned int len = 0;
  unsigned char key[16];

  switch (degree_) {
    case 1:
      for (size_t i1 = 0; i1 < size; ++i1) {
        size_t pos1 = 0;
        p = 0;
        encodeBER(ary[i1], key, &len);
        r = eda_.traverse(reinterpret_cast<const char*>(key), pos1, p, len);
        if (r == -2) continue;
        if (r >= 0) score += (r - kPKEBase);
      }
      break;

    case 2:
      for (size_t i1 = 0; i1 < size; ++i1) {
        size_t pos1 = 0;
        p = 0;
        encodeBER(ary[i1], key, &len);
        r = eda_.traverse(reinterpret_cast<const char*>(key), pos1, p, len);
        if (r == -2) continue;
        if (r >= 0) score += (r - kPKEBase);
        for (size_t i2 = i1+1; i2 < size; ++i2) {
          size_t pos2 = pos1;
          p = 0;
          encodeBER(ary[i2], key, &len);
          r = eda_.traverse(reinterpret_cast<const char*>(key), pos2, p, len);
          if (r == -2) continue;
          if (r >= 0) score += (r - kPKEBase);
        }
      }
      break;

    case 3:
      for (size_t i1 = 0; i1 < size; ++i1) {
        size_t pos1 = 0;
        p = 0;
        encodeBER(ary[i1], key, &len);
        r = eda_.traverse(reinterpret_cast<const char*>(key), pos1, p, len);
        if (r == -2) continue;
        if (r >= 0) score += (r - kPKEBase);
        for (size_t i2 = i1+1; i2 < size; ++i2) {
          size_t pos2 = pos1;
          p = 0;
          encodeBER(ary[i2], key, &len);
          r = eda_.traverse(reinterpret_cast<const char*>(key), pos2, p, len);
          if (r == -2) continue;
          if (r >= 0) score += (r - kPKEBase);
          for (size_t i3 = i2+1; i3 < size; ++i3) {
            size_t pos3 = pos2;
            p = 0;
            encodeBER(ary[i3], key, &len);
            r = eda_.traverse(reinterpret_cast<const char*>(key), pos3, p, len);
            if (r >= 0) score += (r - kPKEBase);
          }
        }
      }
      break;
    default:
      break;
  }

  return score * normalzie_factor_;
}

struct RuleCompare {
  bool operator()(const PKEMine::Rule *f1,
                  const PKEMine::Rule *f2) {
    const unsigned char *p1 = f1->f;
    const unsigned char *p2 = f2->f;
    const unsigned int l = _min(f1->len, f2->len);
    for (size_t i = 0; i < l; i++) {
      if (static_cast<unsigned int>(p1[i])
          > static_cast<unsigned int>(p2[i]))
        return false;
      else if (static_cast<unsigned int>(p1[i])
               < static_cast<unsigned int>(p2[i]))
        return true;
    }
    return f1->len < f2->len;
  }
};

bool SVM::compile(const char *filename, const char *output,
                  float sigma, size_t minsup) {
  progress_timer timer;
  std::ifstream ifs(filename);
  CHECK_DIE(ifs) << "no such file or directory: " << filename;

  std::ofstream bofs(output, std::ios::binary|std::ios::out);
  CHECK_DIE(bofs) << "permission denied: " << output;

  scoped_array<char> buf(new char[BUF_SIZE]);
  scoped_array<char *> column(new char *[BUF_SIZE]);
  unsigned int magic = 0;
  const unsigned int version = MODEL_VERSION;
  unsigned int dsize = 0;
  unsigned int tsize = 0;
  unsigned int degree = 0;
  int bias = 0;
  int maxid = 0;
  unsigned int fsize = 0;
  double normalize_factor = 0.0;

  {
    std::vector<std::pair<std::string, int> > dic;
    while (ifs.getline(buf.get(), BUF_SIZE)) {
      if (std::strlen(buf.get()) == 0) break;
      const size_t size = tokenize(buf.get(), " ", column.get(), 2);
      CHECK_DIE(size == 2) << "format error: " << buf.get();
      const int id = std::atoi(column[0]);
      maxid = _max(maxid, id);
      CHECK_DIE(id >= 0);
      dic.push_back(std::make_pair(std::string(column[1]), id));
    }

    fsize = dic.size();
    Darts::DoubleArray da;
    std::vector<char *> str(dic.size());
    std::vector<Darts::DoubleArray::value_type> val(dic.size());
    std::sort(dic.begin(), dic.end());
    for (size_t i = 0; i < dic.size(); ++i) {
      str[i] = const_cast<char *>(dic[i].first.c_str());
      val[i] = dic[i].second;
    }
    CHECK_DIE(0 ==
              da.build(dic.size(), &str[0], 0, &val[0],
                       &progress_bar_dic))
        << "unkown error in building double-array";

    bofs.write(reinterpret_cast<const char *>(&magic),
               sizeof(magic));
    bofs.write(reinterpret_cast<const char *>(&version),
               sizeof(version));
    bofs.write(reinterpret_cast<const char *>(&dsize),
               sizeof(dsize));
    bofs.write(reinterpret_cast<const char *>(&tsize),
               sizeof(tsize));
    bofs.write(reinterpret_cast<const char *>(&degree),
               sizeof(degree));
    bofs.write(reinterpret_cast<const char *>(&bias),
               sizeof(bias));
    bofs.write(reinterpret_cast<const char *>(&normalize_factor),
               sizeof(normalize_factor));
    bofs.write(reinterpret_cast<const char*>(da.array()),
               da.unit_size() * da.size());
    dsize = da.unit_size() * da.size();
  }

  CHECK_DIE(ifs.getline(buf.get(), BUF_SIZE))  << "format error: " << filename;
  degree = std::atoi(buf.get());
  CHECK_DIE(degree >= 1 && degree <= 3);

  CHECK_DIE(ifs.getline(buf.get(), BUF_SIZE))  << "format error: " << filename;
  double fbias = std::atof(buf.get());


  // PKE mine
  PKEMine pkemine;
  std::vector<PKEMine::Rule*> rules;
  {
    std::vector<std::vector<unsigned int> >  transaction;
    std::vector<float> w;

    while (ifs.getline(buf.get(), BUF_SIZE)) {
      const size_t size = tokenize(buf.get(), " ", column.get(), BUF_SIZE);
      const float alpha = std::atof(column[0]);
      w.push_back(alpha);
      fbias -= WEIGHT[degree][0] * alpha;
      transaction.resize(transaction.size() + 1);
      for (size_t i = 1; i < size; ++i) {
        const unsigned int id = std::atoi(column[i]);
        CHECK_DIE(id <= static_cast<unsigned int>(maxid))
            << "id range is invalid";
        transaction.back().push_back(id);
      }
    }
    pkemine.run(transaction, w, sigma, minsup, degree, &rules);
  }

  // make trie from rules
  {
    std::sort(rules.begin(), rules.end(), RuleCompare());
    std::vector<size_t> len(rules.size());
    std::vector<Darts::DoubleArray::value_type> val(rules.size());
    std::vector<char *> str(rules.size());

    for (size_t i = 0; i < rules.size(); i++) {
      normalize_factor = _max(static_cast<double>(std::abs(rules[i]->w)),
                              normalize_factor);
    }

    normalize_factor /= kPKEBase;
    bias = static_cast<int>(fbias / normalize_factor);

    for (size_t i = 0; i < rules.size(); ++i) {
      len[i] = rules[i]->len;
      str[i] = reinterpret_cast<char *>(rules[i]->f);
      val[i] = static_cast<int>(rules[i]->w / normalize_factor) + kPKEBase;
      CHECK_DIE(val[i] >= 0);
    }

    Darts::DoubleArray da;
    CHECK_DIE(0 ==
              da.build(rules.size(), &str[0], &len[0], &val[0],
                       &progress_bar_trie))
        << "unkown error in building double-array";
    bofs.write(reinterpret_cast<const char*>(da.array()),
               da.unit_size() * da.size());
    tsize = da.unit_size() * da.size();
  }

  magic = static_cast<unsigned int>(bofs.tellp());
  magic ^= DictionaryMagicID;
  bofs.seekp(0);

  bofs.write(reinterpret_cast<const char *>(&magic),
             sizeof(magic));
  bofs.write(reinterpret_cast<const char *>(&version),
             sizeof(version));
  bofs.write(reinterpret_cast<const char *>(&dsize),
             sizeof(dsize));
  bofs.write(reinterpret_cast<const char *>(&tsize),
             sizeof(tsize));
  bofs.write(reinterpret_cast<const char *>(&degree),
             sizeof(degree));
  bofs.write(reinterpret_cast<const char *>(&bias),
             sizeof(bias));
  bofs.write(reinterpret_cast<const char *>(&normalize_factor),
             sizeof(normalize_factor));

  bofs.close();

  std::cout << std::endl;
  std::cout << "double array size : " << dsize << std::endl;
  std::cout << "trie         size : " << tsize << std::endl;
  std::cout << "degree            : " << degree << std::endl;
  std::cout << "minsup            : " << minsup << std::endl;
  std::cout << "bias              : " << bias << std::endl;
  std::cout << "normalize factor  : " << normalize_factor << std::endl;
  std::cout << "rule size         : " << rules.size() << std::endl;
  std::cout << "feature size      : " << fsize << std::endl;
  std::cout << "\nDone! " << std::endl;

  return true;
}

SVMTest::SVMTest(): degree_(2), bias_(0.0) {}
SVMTest::~SVMTest() {}

double SVMTest::classify(const std::vector<int> &ary) const {
  double result = -bias_;
  for (size_t i = 0; i < x_.size(); ++i) {
    result += (w_[i] * kernel(ary, x_[i], degree_));
  }
  return result;
}

double SVMTest::classify(size_t argc, char **argv) {
  std::vector<int> ary;
  for (size_t i = 0; i < argc; ++i) {
    std::map<std::string, int>::const_iterator it = dic_.find(argv[i]);
    if (it != dic_.end()) {
      ary.push_back(it->second);
    }
  }
  std::sort(ary.begin(), ary.end());
  return classify(ary);
}

void SVMTest::close() {
  dic_.clear();
  w_.clear();
  x_.clear();
}

bool SVMTest::open(const char *filename) {
  scoped_array<char *> column(new char *[BUF_SIZE]);
  scoped_array<char> buf(new char[BUF_SIZE]);

  this->close();
  std::ifstream ifs(filename);
  CHECK_DIE(ifs) << "no such file or directory: [" << filename << "]";

  while (ifs.getline(buf.get(), BUF_SIZE)) {
    if (std::strlen(buf.get()) == 0)
      break;
    const size_t size = tokenize(buf.get(), "\t ", column.get(), 2);
    CHECK_DIE(size >= 2);
    dic_.insert(std::make_pair(std::string(column[1]), std::atoi(column[0])));
  }

  CHECK_DIE(ifs.getline(buf.get(), BUF_SIZE));
  degree_ = std::atoi(buf.get());

  CHECK_DIE(ifs.getline(buf.get(), BUF_SIZE));
  bias_ = std::atof(buf.get());

  while (ifs.getline(buf.get(), BUF_SIZE)) {
    const size_t size = tokenize(buf.get(), " ", column.get(), BUF_SIZE);
    if (size < 2) continue;
    w_.push_back(std::atof(column[0]));
    x_.resize(x_.size() + 1);
    for (size_t i = 1; i < size; ++i) {
      x_.back().push_back(std::atoi(column[i]));
    }
    std::sort(x_.back().begin(), x_.back().end());
  }

  return true;
}
}

#if 0
using namespace CaboCha;

int main(int argc, char **argv) {
  const char *input = argv[1];
  const char *output = argv[2];

  CaboCha::SVM::compile(input, output, 0.001, 2);

  //  CaboCha::SVMTest test;
  //  CHECK_DIE(test.open(argv[1]));

  CaboCha::SVM svm;
  CHECK_DIE(svm.open(argv[2]));

  std::ifstream ifs(argv[1]);

  scoped_array<char *> column(new char *[BUF_SIZE]);
  scoped_array<char> buf(new char[BUF_SIZE]);

  // skip
  while (ifs.getline(buf, BUF_SIZE)) {
    if (std::strlen(buf) == 0)
      break;
  }

  CHECK_DIE(ifs.getline(buf, BUF_SIZE));
  CHECK_DIE(ifs.getline(buf, BUF_SIZE));

  std::vector<int> ary;
  while (ifs.getline(buf, BUF_SIZE)) {
    const size_t size = tokenize(buf, " ", column, BUF_SIZE);
    if (size < 2) continue;
    ary.clear();
    for (size_t i = 1; i < size; ++i) {
      ary.push_back(std::atoi(column[i]));
    }
    svm.classify(ary);
  }
}
#endif
