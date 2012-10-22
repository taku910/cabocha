// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "common.h"
#include "darts.h"
#include "freelist.h"
#include "mmap.h"
#include "scoped_ptr.h"
#include "svm.h"
#include "timer.h"
#include "ucs.h"
#include "utils.h"

namespace CaboCha {

inline int dot(const int *x1, const int *x2) {
  int n = 0;
  while (*x1 >= 0 && *x2 >= 0) {
    if (*x1 == *x2) {
      ++n;
      ++x1;
      ++x2;
    } else if (*x1 > *x2) {
      ++x2;
    } else if (*x1 < *x2) {
      ++x1;
    }
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

static const int kWeight[4][5] = {
  {0, 0, 0, 0, 0},   // 0
  {1, 1, 0, 0, 0},   // 1
  {1, 3, 2, 0, 0},   // 2
  {1, 7, 12, 6, 0}   // 3
};

static const int kMaxWeight[5] = { 0, 1, 3, 12 };

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
      w += kWeight[degree_][pattern_.size()] * (*w_)[projected[i].first];
      if ((*w_)[projected[i].first] > 0) {
        mu_pos += kMaxWeight[degree_] * (*w_)[projected[i].first];
      } else {
        mu_neg += kMaxWeight[degree_] * (*w_)[projected[i].first];
      }
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
    if (pattern_.size() >= degree_ || projected.empty()) {
      return;
    }

    std::map<unsigned int, std::vector<std::pair<size_t, int> > > counter;
    for (size_t i = 0; i < projected.size(); ++i) {
      const size_t id   = projected[i].first;
      const int    pos  = projected[i].second;
      const size_t size = (*transaction_)[id].size();
      for (size_t j = pos + 1; j < size; ++j) {
        counter[(*transaction_)[id][j]].push_back
            (std::make_pair(id, j));
      }
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

FastSVMModel::FastSVMModel(): normalzie_factor_(1.0) {}
FastSVMModel::~FastSVMModel() {}

void FastSVMModel::close() {
  mmap_.close();
}

void SVMModelInterface::set_param(const char *key, const char *value) {
  param_[key] = value;
}

void SVMModelInterface::set_param(const char *key, int value) {
  char tmp[BUF_SIZE];
  snprintf(tmp, BUF_SIZE - 1, "%d", value);
  param_[key] = tmp;
}

void SVMModelInterface::set_param(const char *key, double value) {
  char tmp[BUF_SIZE];
  snprintf(tmp, BUF_SIZE - 1, "%f", value);
  param_[key] = tmp;
}

const char *SVMModelInterface::get_param(const char *key) const {
  std::map<std::string, std::string>::const_iterator it = param_.find(key);
  return it == param_.end() ? 0 : it->second.c_str();
}

bool FastSVMModel::open(const char *filename) {
  close();

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

  scoped_fixed_array<char *, BUF_SIZE> column;

  // model parameters
  unsigned int all_psize = 0;
  read_static<unsigned int>(&ptr, all_psize);  // parameter size;
  CHECK_FALSE(all_psize % 8 == 0);
  {
    scoped_array<char> param_buf(new char[all_psize]);
    std::memcpy(param_buf.get(), ptr, all_psize);
    const size_t psize = tokenize(param_buf.get(), "\t",
                                  column.get(), column.size());
    CHECK_FALSE(psize >= 2);
    CHECK_FALSE(psize % 2 == 0);
    for (size_t i = 0; i < psize; i += 2) {
      param_[column[i]] = column[i + 1];
    }
  }
  ptr += all_psize;

  unsigned int dsize = 0;
  unsigned int fsize = 0;
  read_static<double>(&ptr, normalzie_factor_);
  read_static<int>(&ptr, bias_);
  read_static<unsigned int>(&ptr, dsize);  // double array
  read_static<unsigned int>(&ptr, fsize);  // trie

  CHECK_FALSE(dsize != 0);
  CHECK_FALSE(fsize != 0);
  CHECK_FALSE(normalzie_factor_ > 0.0);

  da_.set_array(reinterpret_cast<void *>(const_cast<char *>(ptr)));
  ptr += dsize;
  eda_.set_array(reinterpret_cast<void *>(const_cast<char *>(ptr)));
  ptr += fsize;

  degree_ = std::atoi(get_param("degree"));
  CHECK_FALSE(degree_ >= 1 && degree_ <= 3)
      << "degree must be 1<=degree<=3";

  CHECK_FALSE(ptr == mmap_.end())
      << "dictionary file is broken: " << filename;

  return true;
}

double FastSVMModel::classify(const std::vector<const char *> &features) const {
  std::vector<int> dot_buf;
  dot_buf.reserve(features.size());
  for (size_t i = 0; i < features.size(); ++i) {
    const int r =
        da_.exactMatchSearch<Darts::DoubleArray::result_type>(features[i]);
    if (r != -1) {
      dot_buf.push_back(r);
    }
  }
  std::sort(dot_buf.begin(), dot_buf.end());
  return classify(dot_buf);
}

double FastSVMModel::classify(const std::vector<int> &ary) const {
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
    const unsigned int l = std::min(f1->len, f2->len);
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

bool FastSVMModel::compile(const char *filename, const char *output,
                           double sigma, size_t minsup, Iconv *iconv) {
  progress_timer timer;

  SVMModel model;
  CHECK_DIE(model.open(filename)) << "no such file or directory: " << filename;

  model.set_param("sigma",  sigma);
  model.set_param("minsup", static_cast<int>(minsup));
  model.set_param("charset", encode_charset(iconv->to()));

  std::string param_str;
  Darts::DoubleArray dic_da;
  {
    const std::map<std::string, int> &dic = model.dic();
    const std::map<std::string, std::string> &param = model.param();
    for (std::map<std::string, std::string>::const_iterator it = param.begin();
         it != param.end(); ++it) {
      if (!param_str.empty()) {
        param_str.append("\t");
      }
      param_str.append(it->first);
      param_str.append("\t");
      param_str.append(it->second);
    }
    size_t new_param_str_size = param_str.size();
    while (new_param_str_size % 8 != 0) {
      ++new_param_str_size;
    }
    param_str.resize(new_param_str_size, '\0');

    std::vector<std::pair<std::string, int> > pair_dic;
    for (std::map<std::string, int>::const_iterator it = dic.begin();
         it != dic.end(); ++it) {
      std::string key = it->first;
      if (!iconv->convert(&key)) {
        std::cerr << "iconv conversion failed. skip this entry:"
                  << it->first;
        continue;
      }
      pair_dic.push_back(std::make_pair(key, it->second));
    }
    std::sort(pair_dic.begin(), pair_dic.end());
    std::vector<char *> str;
    std::vector<Darts::DoubleArray::value_type> val;
    for (size_t i = 0; i < pair_dic.size(); ++i) {
      str.push_back(const_cast<char *>(pair_dic[i].first.c_str()));
      val.push_back(pair_dic[i].second);
    }
    CHECK_DIE(0 ==
              dic_da.build(str.size(), &str[0], 0, &val[0],
                           &progress_bar_dic))
        << "unkown error in building double-array";
  }

  Darts::DoubleArray feature_da;
  double normalize_factor = 0.0;
  int bias = 0;
  int degree = 0;
  {
    degree = std::atoi(model.get_param("degree"));
    double fbias = std::atof(model.get_param("bias"));

    CHECK_DIE(2 == degree) << "degree must be 2";
    CHECK_DIE(0.0 == fbias) << "bias must be 0.0";

    // PKE mine
    PKEMine pkemine;
    std::vector<PKEMine::Rule *> rules;
    {
      std::vector<std::vector<unsigned int> >  transaction;
      std::vector<float> w;

      for (size_t i = 0; i < model.size(); ++i) {
        const float alpha = model.alpha(i);
        w.push_back(alpha);
        fbias -= kWeight[degree][0] * alpha;
        transaction.resize(transaction.size() + 1);
        for (const int *x = model.x(i); *x >= 0; ++x) {
          transaction.back().push_back(*x);
        }
      }
      CHECK_DIE(w.size() == transaction.size());
      pkemine.run(transaction, w, sigma, minsup, degree, &rules);
    }

    // make trie from rules
    {
      std::sort(rules.begin(), rules.end(), RuleCompare());
      std::vector<size_t> len(rules.size());
      std::vector<Darts::DoubleArray::value_type> val(rules.size());
      std::vector<char *> str(rules.size());

      for (size_t i = 0; i < rules.size(); i++) {
        normalize_factor = std::max(static_cast<double>(std::abs(rules[i]->w)),
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

      CHECK_DIE(0 ==
                feature_da.build(rules.size(), &str[0], &len[0], &val[0],
                                 &progress_bar_trie))
          << "unkown error in building double-array";
    }
  }

  {
    const unsigned int version = MODEL_VERSION;
    const unsigned int psize = param_str.size();
    const unsigned int dsize = dic_da.unit_size() * dic_da.size();
    const unsigned int fsize = feature_da.unit_size() * feature_da.size();

    unsigned int magic =
        sizeof(version) * 5 +    // (magic,version,psize,dsize,fsize)
        sizeof(normalize_factor) +  // (normalize_factor)
        sizeof(bias) +
        param_str.size() + dsize + fsize;

    magic ^= DictionaryMagicID;

    std::ofstream bofs(WPATH(output), std::ios::binary|std::ios::out);
    CHECK_DIE(bofs) << "permission denied: " << output;

    bofs.write(reinterpret_cast<const char *>(&magic),
               sizeof(magic));
    bofs.write(reinterpret_cast<const char *>(&version),
               sizeof(version));
    bofs.write(reinterpret_cast<const char *>(&psize),
               sizeof(psize));
    bofs.write(param_str.data(), param_str.size());
    bofs.write(reinterpret_cast<const char *>(&normalize_factor),
               sizeof(normalize_factor));
    bofs.write(reinterpret_cast<const char *>(&bias),
               sizeof(bias));
    bofs.write(reinterpret_cast<const char *>(&dsize),
               sizeof(dsize));
    bofs.write(reinterpret_cast<const char *>(&fsize),
               sizeof(fsize));
    bofs.write(reinterpret_cast<const char*>(dic_da.array()), dsize);
    bofs.write(reinterpret_cast<const char*>(feature_da.array()), fsize);

    std::cout << std::endl;
    std::cout << "double array size : " << dsize << std::endl;
    std::cout << "trie         size : " << fsize << std::endl;
    std::cout << "degree            : " << degree << std::endl;
    std::cout << "minsup            : " << minsup << std::endl;
    std::cout << "bias              : " << bias << std::endl;
    std::cout << "normalize factor  : " << normalize_factor << std::endl;
    std::cout << "feature size      : " << model.dic().size() << std::endl;
    std::cout << "Done!\n";
  }

  return true;
}

SVMModel::SVMModel() : feature_freelist_(8192) {}
SVMModel::~SVMModel() {}

void SVMModel::close() {
  alpha_.clear();
  x_.clear();
  dic_.clear();
  param_.clear();
  feature_freelist_.free();
}

double SVMModel::classify(const std::vector<int> &x) const {
  std::vector<int> ary(x);
  std::sort(ary.begin(), ary.end());
  ary.push_back(-1);
  double result = 0.0;
  for (size_t i = 0; i < x_.size(); ++i) {
    const int s = dot(&ary[0], x_[i]);
    result += alpha_[i] * (s + 1) * (s + 1);
  }
  return result;
}

double SVMModel::classify(const std::vector<const char *> &features) const {
  std::vector<int> ary;
  for (size_t i = 0; i < features.size(); ++i) {
    std::map<std::string, int>::const_iterator it = dic_.find(features[i]);
    if (it != dic_.end()) {
      ary.push_back(it->second);
    }
  }
  std::sort(ary.begin(), ary.end());
  return classify(ary);
}

int *SVMModel::alloc(size_t size) {
  return feature_freelist_.alloc(size);
}

bool SVMModel::compress() {
  std::set<int> active_feature;
  for (size_t i = 0; i < size(); ++i) {
    for (const int *fp = x(i); *fp >= 0; ++fp) {
      active_feature.insert(*fp);
    }
  }

  std::map<int, int> old2new;
  int id = 0;
  for (std::map<std::string, int>::const_iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    if (active_feature.find(it->second) != active_feature.end()) {
      if (old2new.find(it->second) == old2new.end()) {
        old2new[it->second] = id;
        ++id;
      }
    }
  }

  std::map<std::string, int> new_dic;
  for (std::map<std::string, int>::const_iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    if (old2new.find(it->second) != old2new.end()) {
      new_dic[it->first] = old2new[it->second];
    }
  }

  for (size_t i = 0; i < size(); ++i) {
    std::vector<int> new_x;
    for (const int *fp = x(i); *fp >= 0; ++fp) {
      CHECK_DIE(old2new.find(*fp) != old2new.end());
      new_x.push_back(old2new[*fp]);
    }
    std::sort(new_x.begin(), new_x.end());
    int *x = const_cast<int *>(x_[i]);
    std::copy(new_x.begin(), new_x.end(), x);
    x[new_x.size()] = -1;
  }

  dic_ = new_dic;

  return true;
}

bool SVMModel::save(const char *filename) const {
  std::ofstream ofs(WPATH(filename));
  CHECK_DIE(ofs) << "no such file or directory: " << filename;

  for (std::map<std::string, std::string>::const_iterator it = param_.begin();
       it != param_.end(); ++it) {
    ofs << it->first << ": " << it->second << '\n';
  }
  ofs << '\n';

  for (std::map<std::string, int>::const_iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    ofs << it->second << ' ' << it->first << '\n';
  }
  ofs << '\n';

  ofs.setf(std::ios::fixed, std::ios::floatfield);
  ofs.precision(16);

  for (size_t i = 0; i < size(); ++i) {
    ofs << alpha_[i];
    for (const int *fp = x_[i]; *fp >= 0; ++fp) {
      ofs << ' ' << *fp;
    }
    ofs << '\n';
  }

  return true;
}

bool SVMModel::open(const char *filename) {
  close();

  scoped_fixed_array<char, BUF_SIZE * 32> buf;
  scoped_fixed_array<char *, BUF_SIZE> column;

  std::ifstream ifs(WPATH(filename));
  CHECK_DIE(ifs) << "no such file or directory: " << filename;

  while (ifs.getline(buf.get(), buf.size())) {
    if (std::strlen(buf.get()) == 0) {
      break;
    }
    const size_t size = tokenize(buf.get(), " ",
                                 column.get(), column.size());
    CHECK_DIE(size >= 2);
    const size_t len = std::strlen(column[0]);
    CHECK_DIE(len >= 2);
    column[0][len - 1] = '\0';   // remove the last ":"
    param_[column[0]] = column[1];
  }

  while (ifs.getline(buf.get(), buf.size())) {
    if (std::strlen(buf.get()) == 0) {
      break;
    }
    const size_t size = tokenize(buf.get(), "\t ", column.get(), 2);
    CHECK_DIE(size >= 2);
    dic_[column[1]] = std::atoi(column[0]);
  }

  while (ifs.getline(buf.get(), buf.size())) {
    const size_t size = tokenize(buf.get(), " ",
                                 column.get(), column.size());
    CHECK_DIE(size >= 2);
    int *fp = alloc(size);
    for (size_t i = 1; i < size; ++i) {
      fp[i - 1] = std::atoi(column[i]);
    }
    std::sort(fp, fp + size - 1);
    fp[size - 1] = -1;
    const double alpha = std::atof(column[0]);
    add(alpha, fp);
  }

  CHECK_DIE(!param_.empty());
  CHECK_DIE(!dic_.empty());
  CHECK_DIE(alpha_.size() == x_.size());

  return true;
}
}

#if 0
using namespace CaboCha;

int main(int argc, char **argv) {
  const char *input = argv[1];
  const char *output = argv[2];

  CaboCha::FastSVMModel::compile(input, output, 0.001, 2);

  //  CaboCha::SVMTest test;
  //  CHECK_DIE(test.open(argv[1]));

  CaboCha::SVM svm;
  CHECK_DIE(svm.open(argv[2]));

  std::ifstream ifs(WPATH(argv[1]));

  scoped_fixed_array<char *, BUF_SIZE> column;
  scoped_fixed_array<char, BUF_SIZE> buf;

  // skip
  while (ifs.getline(buf.get(), buf.size())) {
    if (std::strlen(buf) == 0) {
      break;
    }
  }

  CHECK_DIE(ifs.getline(buf.get(), buf.size()));
  CHECK_DIE(ifs.getline(buf.get(), buf.size()));

  std::vector<int> ary;
  while (ifs.getline(buf.get(), buf.size())) {
    const size_t size = tokenize(buf.get(), " ", column, buf.size());
    if (size < 2) {
      continue;
    }
    ary.clear();
    for (size_t i = 1; i < size; ++i) {
      ary.push_back(std::atoi(column[i]));
    }
    svm.classify(ary);
  }
}
#endif
