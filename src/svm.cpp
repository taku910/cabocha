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
namespace {
struct FeatureKey {
  unsigned char id[7];
  unsigned int len : 8;
};

const unsigned int kDictionaryMagicID = 0xef522177u;
const int kPKEBase = 0xfffff;  // 1048575

inline int dot(const std::vector<int> &x1, const std::vector<int> &x2) {
  int n = 0;
  size_t i1 = 0;
  size_t i2 = 0;
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
  ++value;
  output[(*length)++] = (value & 0x7f);
  while (value >>= 7) {
    ++value;
    output[*length - 1] |= 0x80;
    output[(*length)++] = (value & 0x7f);
  }
}

std::string encodeBER(unsigned int value) {
  unsigned char buf[16];
  unsigned int len = 0;
  encodeBER(value, buf, &len);
  return std::string(reinterpret_cast<const char *>(buf), len);
}

uint64 encodeToUint64(int i1, int i2) {
  return (static_cast<uint64>(i1) << 32 |
          static_cast<uint64>(i2));
}

void decodeFromUint64(uint64 n, unsigned int *i1, unsigned int *i2) {
  *i1 = static_cast<int>(0xFFFFFFFF & n >> 32);
  *i2 = static_cast<int>(0xFFFFFFFF & n);
}

std::string encodeFeatureID(int i1, int i2) {
  return encodeBER(i1) + encodeBER(i2);
}
}  // namespace

FastSVMModel::FastSVMModel()
    : degree_(0), bias_(0), normalize_factor_(0.0),
      feature_size_(0), freq_feature_size_(0),
      node_pos_(0), weight1_(0), weight2_(0) {}

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
  CHECK_FALSE((magic ^ kDictionaryMagicID) == mmap_.size())
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

  unsigned int dic_da_size = 0;
  unsigned int feature_da_size = 0;

  read_static<float>(&ptr, normalize_factor_);
  read_static<int>(&ptr, bias_);
  read_static<unsigned int>(&ptr, feature_size_);  // trie
  read_static<unsigned int>(&ptr, freq_feature_size_);  // trie
  read_static<unsigned int>(&ptr, dic_da_size);  // double array
  read_static<unsigned int>(&ptr, feature_da_size);  // trie

  CHECK_FALSE(feature_size_ > 0);
  CHECK_FALSE(freq_feature_size_ > 0);
  CHECK_FALSE(dic_da_size > 0);
  CHECK_FALSE(feature_da_size > 0);
  CHECK_FALSE(normalize_factor_ > 0.0);

  dic_da_.set_array(reinterpret_cast<void *>(const_cast<char *>(ptr)));
  ptr += dic_da_size;

  feature_da_.set_array(reinterpret_cast<void *>(const_cast<char *>(ptr)));
  ptr += feature_da_size;

  node_pos_ = reinterpret_cast<unsigned int *>(const_cast<char *>(ptr));
  ptr += sizeof(node_pos_[0]) * feature_size_;

  weight1_= reinterpret_cast<int *>(const_cast<char *>(ptr));
  ptr += sizeof(weight1_[0]) * feature_size_;

  weight2_ = reinterpret_cast<int *>(const_cast<char *>(ptr));
  ptr += sizeof(weight2_[0]) *
      (freq_feature_size_ * (freq_feature_size_ - 1)) / 2;

  const char *sdegree = get_param("degree");
  CHECK_FALSE(sdegree) << "degree is not defined";
  degree_ = std::atoi(sdegree);
  CHECK_FALSE(degree_ == 2) << "degree must be 1<=degree<=3";
  CHECK_FALSE(ptr == mmap_.end())
      << "dictionary file is broken: " << filename;

  return true;
}

int FastSVMModel::id(const std::string &key) const {
  return dic_da_.exactMatchSearch<Darts::DoubleArray::result_type>(key.c_str());
}

double FastSVMModel::real_classify(const std::vector<float> &real_x) const {
  CHECK_DIE(false) << "not implemented";
  return 0.0;
}

double FastSVMModel::classify(const std::vector<int> &x) const {
  const size_t size = x.size();
  int score = -bias_;

  size_t freq_size = 0;
  std::vector<FeatureKey> key(size);
  for (size_t i = 0; i < size; ++i) {
    if (x[i] < static_cast<int>(freq_feature_size_)) {
      freq_size = i;
    }
    unsigned int len = 0;
    encodeBER(x[i], key[i].id, &len);
    key[i].len = len;
  }
  ++freq_size;

  const int kOffset = 2 * freq_feature_size_ - 3;
  for (size_t i1 = 0; i1 < freq_size; ++i1) {
    score += weight1_[x[i1]];
    const int *w = &weight2_[x[i1] * (kOffset - x[i1]) / 2 - 1];
    for (size_t i2 = i1 + 1; i2 < freq_size; ++i2) {
      score += w[x[i2]];
    }
  }

  for (size_t i1 = 0; i1 < freq_size; ++i1) {
    const size_t node_pos = node_pos_[x[i1]];
    if (node_pos == 0) {
      continue;
    }
    for (size_t i2 = freq_size; i2 < size; ++i2) {
      size_t node_pos2 = node_pos;
      size_t key_pos = 0;
      const int result = feature_da_.traverse(
          reinterpret_cast<const char *>(key[i2].id),
          node_pos2, key_pos, key[i2].len);
      if (result >= 0) {
        score += (result - kPKEBase);
      }
    }
  }

  for (size_t i1 = freq_size; i1 < size; ++i1) {
    score += weight1_[x[i1]];
    const size_t node_pos = node_pos_[x[i1]];
    if (node_pos == 0) {
      continue;
    }
    for (size_t i2 = i1 + 1; i2 < size; ++i2) {
      size_t node_pos2 = node_pos;
      size_t key_pos = 0;
      const int result = feature_da_.traverse(
          reinterpret_cast<const char *>(key[i2].id),
          node_pos2, key_pos, key[i2].len);
      if (result >= 0) {
        score += (result - kPKEBase);
      }
    }
  }

  return score * normalize_factor_;
}

bool FastSVMModel::compile(const char *filename, const char *output,
                           double sigma, size_t minsup,
                           size_t freq_feature_size,
                           Iconv *iconv) {
  progress_timer timer;

  SVMModel model;
  CHECK_DIE(model.open(filename)) << "no such file or directory: " << filename;

  model.set_param("sigma",  sigma);
  model.set_param("minsup", static_cast<int>(minsup));
  model.set_param("charset", encode_charset(iconv->to()));

  std::string param_str;
  Darts::DoubleArray dic_da;
  Darts::DoubleArray feature_da;
  std::vector<int> weight1, weight2;
  std::vector<unsigned int> node_pos;
  float normalize_factor = 0.0;
  size_t feature_size = 0;
  //  size_t freq_feature_size = 3000;
  int bias = 0;

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
    std::vector<char *> str(pair_dic.size());
    std::vector<Darts::DoubleArray::value_type> val(pair_dic.size());
    for (size_t i = 0; i < pair_dic.size(); ++i) {
      str[i] = const_cast<char *>(pair_dic[i].first.c_str());
      val[i] = pair_dic[i].second;
    }
    CHECK_DIE(0 ==
              dic_da.build(str.size(), &str[0], 0, &val[0],
                           &progress_bar_dic))
        << "unkown error in building double-array";
  }

  {
    const char *sdegree = model.get_param("degree");
    const char *sbias = model.get_param("bias");
    CHECK_DIE(sdegree) << "degree is not defined";
    CHECK_DIE(sbias) << "bias is not defined";
    const int degree = std::atoi(sdegree);
    double fbias = std::atof(sbias);

    CHECK_DIE(2 == degree) << "degree must be 2";
    CHECK_DIE(0.0 == fbias) << "bias must be 0.0";

    for (size_t i = 0; i < model.size(); ++i) {
      const std::vector<int> &x = model.x(i);
      for (size_t i1 = 0; i1 < x.size(); ++i1) {
        feature_size = std::max(feature_size, static_cast<size_t>(x[i1]));
      }
    }
    CHECK_DIE(feature_size != 0);
    ++feature_size;

    freq_feature_size = std::min(freq_feature_size, feature_size);

    std::vector<float> fweight1(feature_size, 0.0);
    std::vector<float> fweight2(freq_feature_size *
                                (freq_feature_size - 1) / 2, 0.0);
    std::vector<std::pair<std::string, float> > feature_trie_output;

    // 0th-degree feature (bias)
    for (size_t i = 0; i < model.size(); ++i) {
      const float alpha = model.alpha(i);
      fbias -= alpha;
    }

    // 1st-degree feature
    for (size_t i = 0; i < model.size(); ++i) {
      const std::vector<int> &x = model.x(i);
      const float alpha = model.alpha(i);
      for (size_t i1 = 0; i1 < x.size(); ++i1) {
        fweight1[x[i1]] += 3 * alpha;
      }
    }

    // 2nd-degree feature
    {
      // This part can be replaced with buscket mining (prefixspan) algorithm.
      hash_map<uint64, std::pair<unsigned char, float> > pair_weight;
      for (size_t i = 0; i < model.size(); ++i) {
        const std::vector<int> &x = model.x(i);
        const float alpha = model.alpha(i);
        for (size_t i1 = 0; i1 < x.size(); ++i1) {
          for (size_t i2 = i1 + 1; i2 < x.size(); ++i2) {
            CHECK_DIE(x[i1] < x[i2]);
            const uint64 key = encodeToUint64(x[i1], x[i2]);
            std::pair<unsigned char, float> *elm = &pair_weight[key];
            elm->first++;
            elm->second += 2 * alpha;
          }
        }
      }

      size_t pos_num = 0;
      size_t neg_num = 0;
      for (size_t i = 0; i < model.size(); ++i) {
        if (model.alpha(i) > 0) {
          ++pos_num;
        } else {
          ++neg_num;
        }
      }

      const float sigma_pos =  1.0 * sigma * pos_num / (pos_num + neg_num);
      const float sigma_neg = -1.0 * sigma * neg_num / (pos_num + neg_num);
      CHECK_DIE(sigma_pos >= 0.0);
      CHECK_DIE(sigma_neg <= 0.0);
      CHECK_DIE(sigma_neg <= sigma_pos);

      // extract valid patterns only.
      for (hash_map<uint64, std::pair<unsigned char, float> >::const_iterator
               it = pair_weight.begin(); it != pair_weight.end(); ++it) {
        const size_t freq = static_cast<size_t>(it->second.first);
        const float w = it->second.second;
        unsigned int i1 = 0;
        unsigned int i2 = 0;
        decodeFromUint64(it->first, &i1, &i2);
        CHECK_DIE(i1 < i2) << i1 << " " << i2;
        CHECK_DIE(i1 < feature_size);
        CHECK_DIE(i2 < feature_size);
        if (i1 < freq_feature_size && i2 < freq_feature_size) {
          const size_t index =
              i1 * (2 * freq_feature_size - 3 - i1) / 2 - 1 + i2;
          CHECK_DIE(index >= 0 && index < fweight2.size());
          fweight2[index] = w;
        } else if (freq >= minsup && (w <= sigma_neg || w >= sigma_pos)) {
          const std::string key = encodeFeatureID(i1, i2);
          feature_trie_output.push_back(std::make_pair(key, w));
        }
      }
    }

    for (size_t i = 0; i < fweight1.size(); ++i) {
      normalize_factor = std::max(std::abs(fweight1[i]), normalize_factor);
    }

    for (size_t i = 0; i < fweight2.size(); ++i) {
      normalize_factor = std::max(std::abs(fweight2[i]), normalize_factor);
    }

    for (size_t i = 0; i < feature_trie_output.size(); ++i) {
      normalize_factor = std::max(std::abs(feature_trie_output[i].second),
                                  normalize_factor);
    }

    normalize_factor /= kPKEBase;
    bias = static_cast<int>(fbias / normalize_factor);

    weight1.resize(fweight1.size(), 0);
    weight2.resize(fweight2.size(), 0);
    node_pos.resize(feature_size, 0);

    for (size_t i = 0; i < fweight1.size(); ++i) {
      weight1[i] = static_cast<int>(fweight1[i] / normalize_factor);
    }

    for (size_t i = 0; i < fweight2.size(); ++i) {
      weight2[i] = static_cast<int>(fweight2[i] / normalize_factor);
    }

    std::sort(feature_trie_output.begin(), feature_trie_output.end());
    std::vector<size_t> len(feature_trie_output.size());
    std::vector<Darts::DoubleArray::value_type> val(feature_trie_output.size());
    std::vector<char *> str(feature_trie_output.size());

    for (size_t i = 0; i < feature_trie_output.size(); ++i) {
      len[i] = feature_trie_output[i].first.size();
      str[i] = const_cast<char *>(feature_trie_output[i].first.c_str());
      val[i] = static_cast<int>(
          feature_trie_output[i].second / normalize_factor) +
          kPKEBase;
      CHECK_DIE(val[i] >= 0);
    }

    CHECK_DIE(0 ==
              feature_da.build(feature_trie_output.size(),
                               &str[0], &len[0], &val[0],
                               &progress_bar_trie))
        << "unkown error in building double-array";

    for (size_t i = 0; i < feature_size; ++i) {
      std::string key = encodeBER(i);
      CHECK_DIE(key.size() <= 3);
      size_t key_pos = 0;
      size_t n_pos = 0;
      const int result = feature_da.traverse(key.c_str(),
                                             n_pos, key_pos, key.size());
      CHECK_DIE(result == -1 || result == -2);
      if (result == -1) {
        CHECK_DIE(n_pos > 0);
        node_pos[i] = n_pos;
      }
    }

    CHECK_DIE(weight1.size() == feature_size);
    CHECK_DIE(weight2.size() ==
              (freq_feature_size * (freq_feature_size - 1) / 2));
    CHECK_DIE(node_pos.size() == feature_size);

    CHECK_DIE(weight1.size() > 0);
    CHECK_DIE(weight2.size() > 0);
    CHECK_DIE(node_pos.size() > 0);
  }

  {
    const unsigned int version = MODEL_VERSION;
    const unsigned int param_size = param_str.size();
    const unsigned int feature_size2 = feature_size;
    const unsigned int freq_feature_size2 = freq_feature_size;
    const unsigned int dic_da_size = dic_da.unit_size() * dic_da.size();
    const unsigned int feature_da_size =
        feature_da.unit_size() * feature_da.size();

    unsigned int file_size =
        sizeof(version) * 7 +
        // (magic,version,param_size,feature_size2,freq_feature_size2,
        // dic_da_size,feature_da_size)
        sizeof(normalize_factor) +  // (normalize_factor)
        sizeof(bias) +
        param_str.size() +
        dic_da_size + feature_da_size +
        node_pos.size() * sizeof(node_pos[0]) +
        weight1.size() * sizeof(weight1[0]) +
        weight2.size() * sizeof(weight2[0]);

    unsigned int magic = file_size ^ kDictionaryMagicID;

    std::ofstream bofs(WPATH(output), std::ios::binary|std::ios::out);
    CHECK_DIE(bofs) << "permission denied: " << output;

    bofs.write(reinterpret_cast<const char *>(&magic),
               sizeof(magic));
    bofs.write(reinterpret_cast<const char *>(&version),
               sizeof(version));
    bofs.write(reinterpret_cast<const char *>(&param_size),
               sizeof(param_size));
    bofs.write(param_str.data(), param_str.size());
    bofs.write(reinterpret_cast<const char *>(&normalize_factor),
               sizeof(normalize_factor));
    bofs.write(reinterpret_cast<const char *>(&bias),
               sizeof(bias));
    bofs.write(reinterpret_cast<const char *>(&feature_size2),
               sizeof(feature_size2));
    bofs.write(reinterpret_cast<const char *>(&freq_feature_size2),
               sizeof(freq_feature_size2));
    bofs.write(reinterpret_cast<const char *>(&dic_da_size),
               sizeof(dic_da_size));
    bofs.write(reinterpret_cast<const char *>(&feature_da_size),
               sizeof(feature_da_size));
    bofs.write(reinterpret_cast<const char *>(dic_da.array()), dic_da_size);
    bofs.write(reinterpret_cast<const char *>(feature_da.array()),
               feature_da_size);
    bofs.write(reinterpret_cast<const char *>(&node_pos[0]),
               node_pos.size() * sizeof(node_pos[0]));
    bofs.write(reinterpret_cast<const char *>(&weight1[0]),
               weight1.size() * sizeof(weight1[0]));
    bofs.write(reinterpret_cast<const char *>(&weight2[0]),
               weight2.size() * sizeof(weight2[0]));

    CHECK_DIE(file_size  == static_cast<size_t>(bofs.tellp()));

    std::cout << std::endl;
    std::cout << "double array size : " << dic_da_size << std::endl;
    std::cout << "trie         size : " << feature_da_size << std::endl;
    std::cout << "feature size      : " << feature_size << std::endl;
    std::cout << "freq feature size : " << freq_feature_size << std::endl;
    std::cout << "minsup            : " << minsup << std::endl;
    std::cout << "bias              : " << bias << std::endl;
    std::cout << "sigma             : " << sigma << std::endl;
    std::cout << "normalize factor  : " << normalize_factor << std::endl;
    std::cout << "Done!\n";
  }

  return true;
}

SVMModel::SVMModel() {}
SVMModel::~SVMModel() {}

void SVMModel::close() {
  alpha_.clear();
  x_.clear();
  real_x_.clear();
  real_weight_.clear();
  dic_.clear();
  param_.clear();
}

double SVMModel::classify(const std::vector<int> &x) const {
  double result = 0.0;
  for (size_t i = 0; i < x_.size(); ++i) {
    const int s = dot(x, x_[i]);
    result += alpha_[i] * (s + 1) * (s + 1);
  }
  return result;
}

double SVMModel::real_classify(const std::vector<float> &real_x) const {
  CHECK_DIE(real_weight_.size() == real_x.size());
  double result = 0.0;
  for (size_t i = 0; i < real_x.size(); ++i) {
    result += real_weight_[i] * real_x[i];
  }
  return result;
}

bool SVMModel::compress() {
  return compress(0);
}

bool SVMModel::compress(size_t freq) {
  std::map<int, size_t> active_feature;
  for (size_t i = 0; i < size(); ++i) {
    for (size_t j = 0; j < x_[i].size(); ++j) {
      active_feature[x_[i][j]]++;
    }
  }

  std::vector<std::string> dic_vec(dic_.size());
  for (std::map<std::string, int>::const_iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    dic_vec[it->second] = it->first;
  }

  int id = 0;
  std::map<int, int> old2new;
  std::map<std::string, int> new_dic;
  for (size_t i = 0; i < dic_vec.size(); ++i) {
    std::map<int, size_t>::const_iterator it = active_feature.find(i);
    if (it != active_feature.end() && it->second >= freq) {
      old2new[i] = id;
      new_dic[dic_vec[i]] = id;
      ++id;
    }
  }

  for (size_t i = 0; i < size(); ++i) {
    std::vector<int> new_x;
    for (size_t j = 0; j < x_[i].size(); ++j) {
      CHECK_FALSE(old2new.find(x_[i][j]) != old2new.end());
      new_x.push_back(old2new[x_[i][j]]);
    }
    std::sort(new_x.begin(), new_x.end());
    CHECK_DIE(!new_x.empty()) << "|freq| seems to be too small.";
    x_[i] = new_x;
  }

  dic_ = new_dic;

  return true;
}

bool SVMModel::sortInstances() {
  std::vector<std::pair<std::vector<int>, size_t> > tmp(x_.size());
  for (size_t i = 0; i < x_.size(); ++i) {
    tmp[i].first = x_[i];
    tmp[i].second = i;
  }
  std::stable_sort(tmp.begin(), tmp.end());

  std::vector<double> alpha(tmp.size());
  for (size_t i = 0; i < tmp.size(); ++i) {
    x_[i] = tmp[i].first;
    alpha[i] = alpha_[tmp[i].second];
  }

  alpha_ = alpha;
  return true;
}

bool SVMModel::sortFeatures() {
  int max_id = 0;
  for (size_t i = 0; i < size(); ++i) {
    for (size_t j = 0; j < x_[i].size(); ++j) {
      max_id = std::max(max_id, x_[i][j]);
    }
  }

  std::vector<std::pair<int, int> > freq(max_id + 1);
  for (size_t i = 0; i < size(); ++i) {
    for (size_t j = 0; j < x_[i].size(); ++j) {
      freq[x_[i][j]].first++;
      freq[x_[i][j]].second = x_[i][j];
    }
  }

  // sort by freq. if freq is the same, keep the original
  // order as much as possible.
  std::stable_sort(freq.begin(), freq.end(),
                   std::greater<std::pair<int, int> >());

  std::vector<int> old2new(freq.size(), -1);
  for (size_t i = 0; i < freq.size(); ++i) {
    old2new[freq[i].second] = i;
  }

  for (size_t i = 0; i < old2new.size(); ++i) {
    CHECK_DIE(old2new[i] >= 0);
  }

  for (std::map<std::string, int>::iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    it->second = old2new[it->second];
  }

  for (size_t i = 0; i < size(); ++i) {
    for (size_t j = 0; j < x_[i].size(); ++j) {
      x_[i][j] = old2new[x_[i][j]];
    }
  }

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
    if (!real_x_.empty()) {
      CHECK_DIE(real_x_.size() == size());
      for (size_t j = 0; j < real_x_[i].size(); ++j) {
        ofs << ' ' << real_x_[i][j];
      }
      ofs << " |||";
    }
    for (size_t j = 0; j < x_[i].size(); ++j) {
      ofs << ' ' << x_[i][j];
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
  CHECK_FALSE(ifs) << "no such file or directory: " << filename;

  while (ifs.getline(buf.get(), buf.size())) {
    if (std::strlen(buf.get()) == 0) {
      break;
    }
    const size_t size = tokenize(buf.get(), " ",
                                 column.get(), column.size());
    CHECK_FALSE(size >= 2);
    const size_t len = std::strlen(column[0]);
    CHECK_FALSE(len >= 2);
    column[0][len - 1] = '\0';   // remove the last ":"
    param_[column[0]] = column[1];
  }

  while (ifs.getline(buf.get(), buf.size())) {
    if (std::strlen(buf.get()) == 0) {
      break;
    }
    const size_t size = tokenize(buf.get(), "\t ", column.get(), 2);
    CHECK_FALSE(size >= 2);
    dic_[column[1]] = std::atoi(column[0]);
  }

  while (ifs.getline(buf.get(), buf.size())) {
    const size_t size = tokenize(buf.get(), " ",
                                 column.get(), column.size());
    int real_weight_feature_size = -1;
    for (size_t i = 1; i < size; ++i) {
      if (std::strcmp(column[i], "|||") == 0) {
        real_weight_feature_size = i;
        break;
      }
    }

    const double alpha = std::atof(column[0]);
    std::vector<float> real_x;
    std::vector<int> x;

    if (real_weight_feature_size == -1) {
      for (size_t i = 1; i < size; ++i) {
        x.push_back(std::atoi(column[i]));
      }
    } else {
      for (int i = 1; i < real_weight_feature_size; ++i) {
        real_x.push_back(std::atof(column[i]));
      }
      for (int i = real_weight_feature_size + 1;
           i < static_cast<int>(size); ++i) {
        x.push_back(std::atoi(column[i]));
      }
    }

    std::sort(x.begin(), x.end());
    CHECK_DIE(!x.empty());
    add(alpha, x, real_x);
  }

  CHECK_FALSE(!param_.empty());
  CHECK_FALSE(!dic_.empty());
  CHECK_FALSE(alpha_.size() == x_.size());
  CHECK_FALSE(!alpha_.empty());

  if (!real_x_.empty()) {
    CHECK_FALSE(real_x_.size() == x_.size());
    real_weight_.resize(real_x_.front().size(), 0.0);
    for (size_t i = 0; i < size(); ++i) {
      CHECK_DIE(real_weight_.size() == real_x_[i].size());
      for (size_t j = 0; j < real_weight_.size(); ++j) {
        real_weight_[j] += alpha(i) * real_x_[i][j];
      }
    }
  }

  return true;
}

void SVMModel::add(double alpha, const std::vector<int> &x) const {
  CHECK_DIE(real_x_.empty());
  alpha_.push_back(alpha);
  x_.push_back(x);
}

void SVMModel::add(double alpha, const std::vector<int> &x,
                   const std::vector<float> &real_x) const {
  if (real_x.empty()) {
    add(alpha, x);
    return;
  }
  CHECK_DIE(x_.size() == real_x_.size()) << x_.size() << " " << real_x_.size();
  alpha_.push_back(alpha);
  x_.push_back(x);
  // When using this method, keep to use this method.
  if (real_x_.size() > 0) {
    CHECK_DIE(real_x_.back().size() == real_x.size())
        << real_x_.back().size() << " " << real_x.size();
  }
  real_x_.push_back(real_x);
};

int SVMModel::id(const std::string &key) const {
  std::map<std::string, int>::const_iterator it = dic_.find(key);
  if (it != dic_.end()) {
    return it->second;
  }
  const int id = dic_.size();
  dic_.insert(std::make_pair(key, id));
  return id;
}

int ImmutableSVMModel::id(const std::string &key) const {
  std::map<std::string, int>::const_iterator it = dic_.find(key);
  if (it != dic_.end()) {
    return it->second;
  }
  return -1;
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

  std::vector<int> x;
  while (ifs.getline(buf.get(), buf.size())) {
    const size_t size = tokenize(buf.get(), " ", column, buf.size());
    if (size < 2) {
      continue;
    }
    x.clear();
    for (size_t i = 1; i < size; ++i) {
      x.push_back(std::atoi(column[i]));
    }
    svm.classify(x);
  }
}
#endif
