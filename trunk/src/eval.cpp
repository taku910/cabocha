// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: eval.cpp 50 2009-05-03 08:25:36Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include "param.h"
#include "cabocha.h"
#include "common.h"
#include "utils.h"

namespace CaboCha {

const char ALL_TAG[] = "__ALL__";

struct NeChunk {
  std::string category;
  size_t begin;
  size_t size;
};

void find_ne_chunks(const Tree &tree,
                    std::vector<NeChunk> *chunks) {
  CHECK_DIE(chunks);
  chunks->clear();
  for (size_t i = 0; i < tree.token_size(); ++i) {
    const Token *token = tree.token(i);
    const char *ne = token->ne;
    CHECK_DIE(ne && strlen(ne) >= 1) << "NE tag is empty";
    if (ne[0] == 'O') continue;
    CHECK_DIE(strlen(ne) >= 3) << "NE tag is empty";
    CHECK_DIE(ne[1] == '-') << "NE must be formetted as B-NE: " << ne;
    const char stat = ne[0];
    if (stat == 'B') {
      chunks->resize(chunks->size() + 1);
      chunks->back().category = ne + 2;
      chunks->back().begin = i;
      chunks->back().size = 0;
    } else if (stat == 'I') {
      CHECK_DIE(chunks->size() > 1 &&
                chunks->back().category
                == std::string(ne + 2))
          "starting with I tag. Must start with B";
      chunks->back().size++;
    } else {
      std::cerr << "Unknown tag: " << stat;
    }
  }
}

class Eval {
 public:
  static bool eval(int argc, char **argv) {
    static const CaboCha::Option long_options[] = {
      { "parser-type",  'e',  "dep",    "STR", "choose from ne/chunk/dep" },
      { "version",  'v',  0,   0,    "show the version and exit" },
      { "help",  'h',  0,   0,    "show this help and exit." },
      { 0, 0, 0, 0 }
    };

    CaboCha::Param param;
    param.open(argc, argv, long_options);
    if (!param.open(argc, argv, long_options)) {
      std::cout << param.what() << "\n\n" <<  COPYRIGHT
                << "\ntry '--help' for more information." << std::endl;
      return -1;
    }

    if (!param.help_version()) return 0;
    const std::vector<std::string> &files = param.rest_args();
    if (files.size() < 2) {
      std::cout << "Usage: " <<
          param.program_name()
                << "-e [chunk|dep|ne] output answer"
                << std::endl;
      return -1;
    }

    const std::string system = files[0];
    const std::string answer = files[1];
    const std::string stype = param.get<std::string>("parser-type");
    const int type = parser_type(stype.c_str());
    CHECK_DIE(type != -1) << "unknown parser type: " << stype;

    switch (type) {
      case TRAIN_DEP:
        return eval_dep(system.c_str(), answer.c_str());
      case TRAIN_CHUNK:
        return eval_chunk(system.c_str(), answer.c_str());
      case TRAIN_NE:
        return eval_ne(system.c_str(), answer.c_str());
    }

    return 0;
  };

 private:
  static bool eval_chunk(const char *result_file,
                         const char *answer_file) {
    std::ifstream ifs1(result_file);
    std::ifstream ifs2(answer_file);
    CHECK_DIE(ifs1) << "no such file or directory: " << result_file;
    CHECK_DIE(ifs2) << "no such file or directory: " << answer_file;

    std::string tree_str1;
    std::string tree_str2;
    Tree tree1;
    Tree tree2;
    size_t correct = 0;
    size_t prec = 0;
    size_t recall = 0;

    while (ifs1 && ifs2) {
      tree_str1.clear();
      tree_str2.clear();
      CHECK_DIE(read_sentence(&ifs1,
                              &tree_str1,
                              INPUT_DEP));
      CHECK_DIE(read_sentence(&ifs2,
                              &tree_str2,
                              INPUT_DEP));
      CHECK_DIE(tree1.read(tree_str1.c_str(),
                           tree_str1.size(),
                           INPUT_DEP)) << "cannot parse sentence";
      CHECK_DIE(tree2.read(tree_str2.c_str(),
                           tree_str2.size(),
                           INPUT_DEP)) << "cannot parse sentence";

      CHECK_DIE(tree1.token_size() == tree2.token_size())
          << "Token size is different";

      size_t i1 = 0;
      size_t i2 = 0;
      while (i1 < tree1.chunk_size() && i2 < tree2.chunk_size()) {
        const Chunk *chunk1 = tree1.chunk(i1);
        const Chunk *chunk2 = tree2.chunk(i2);
        const size_t t1 = chunk1->token_pos;
        const size_t t2 = chunk2->token_pos;
        if (t1 == t2) {
          if (chunk1->token_size == chunk2->token_size) {
            ++correct;
          }
          ++prec;
          ++recall;
          ++i1;
          ++i2;
        } else if (t1 < t2) {
          ++i1;
          ++prec;
        } else if (t1 > t2) {
          ++i2;
          ++recall;
        }
      }
      while (i1 < tree1.chunk_size()) {
        ++prec;
        ++i1;
      }
      while (i2 < tree2.chunk_size()) {
        ++recall;
        ++i2;
      }
    }

    const double pr = (prec == 0) ? 0 : 100.0 * correct/prec;
    const double re = (recall == 0) ? 0 : 100.0 * correct/recall;
    const double F = ((pr + re) == 0.0) ? 0 : 2 * pr * re /(pr + re);
    char buf[8192];
    std::cout <<  "             precision              recall             F"
              << std::endl;
    snprintf(buf, sizeof(buf) - 1,
             "Chunk: %4.4f (%d/%d) %4.4f (%d/%d) %4.4f\n",
             pr,
             static_cast<int>(correct),
             static_cast<int>(prec),
             re,
             static_cast<int>(correct),
             static_cast<int>(recall),
             F);
    std::cout << buf;
    return true;
  }

  static bool eval_ne(const char *result_file,
                      const char *answer_file) {
    std::ifstream ifs1(result_file);
    std::ifstream ifs2(answer_file);
    CHECK_DIE(ifs1) << "no such file or directory: " << result_file;
    CHECK_DIE(ifs2) << "no such file or directory: " << answer_file;

    std::string tree_str1;
    std::string tree_str2;
    Tree tree1;
    Tree tree2;
    std::map<std::string, size_t> prec;
    std::map<std::string, size_t> recall;
    std::map<std::string, size_t> correct;
    std::set<std::string> categories;
    size_t all = 0;
    size_t acc = 0;
    categories.insert(ALL_TAG);

    while (ifs1 && ifs2) {
      tree_str1.clear();
      tree_str2.clear();
      CHECK_DIE(read_sentence(&ifs1,
                              &tree_str1,
                              INPUT_DEP));
      CHECK_DIE(read_sentence(&ifs2,
                              &tree_str2,
                              INPUT_DEP));
      CHECK_DIE(tree1.read(tree_str1.c_str(),
                           tree_str1.size(),
                           INPUT_DEP)) << "cannot parse sentence";
      CHECK_DIE(tree2.read(tree_str2.c_str(),
                           tree_str2.size(),
                           INPUT_DEP)) << "cannot parse sentence";

      CHECK_DIE(tree1.token_size() == tree2.token_size())
          << "Token size is different";

      std::vector<NeChunk> ne1;
      std::vector<NeChunk> ne2;
      find_ne_chunks(tree1, &ne1);
      find_ne_chunks(tree2, &ne2);

      for (size_t i = 0; i < tree1.size(); ++i) {
        if (std::strcmp(tree1.token(i)->ne,
                        tree2.token(i)->ne) == 0) {
          ++acc;
        }
        ++all;
      }

      size_t i1 = 0;
      size_t i2 = 0;
      while (i1 < ne1.size() && i2 < ne2.size()) {
        const size_t t1 = ne1[i1].begin;
        const size_t t2 = ne2[i2].begin;
        const std::string &c1 = ne1[i1].category;
        const std::string &c2 = ne2[i2].category;
        categories.insert(c1);
        categories.insert(c2);
        if (t1 == t2) {
          if (ne2[i1].size == ne2[i2].size) {
            prec[ALL_TAG]++;
            if (c1 == c2) {
              ++prec[c1];
            }
          }
          recall[ALL_TAG]++;
          correct[ALL_TAG]++;
          ++recall[c1];
          ++correct[c1];
          ++i1;
          ++i2;
        } else if (t1 < t2) {
          ++i1;
          ++prec[c2];
        } else if (t1 > t2) {
          ++i2;
          ++recall[c1];
        }
      }
      while (i1 < ne1.size()) {
        ++prec[ne1[i1].category];
        ++i1;
      }
      while (i2 < ne2.size()) {
        ++recall[ne2[i2].category];
        ++i2;
      }
    }

    char buf[8192];
    const double ac = (all == 0) ? 0.0 : 100.0 * acc / all;
    snprintf(buf, sizeof(buf) - 1,
             "Tag Accuracy:  %4.4f (%d/%d)\n",
             ac, static_cast<int>(all), static_cast<int>(acc));
    std::cout << buf;

    for (std::set<std::string>::const_iterator it = categories.begin();
         it != categories.end(); ++it) {
      const size_t c = correct[*it];
      const size_t p = prec[*it];
      const size_t r  = recall[*it];
      const double pr = (p == 0) ? 0 : 100.0 * c / p;
      const double re = (r == 0) ? 0 : 100.0 * c / r;
      const double F = ((pr + re) == 0.0) ? 0 : 2 * pr * re /(pr + re);
      const char *category = (*it == ALL_TAG ? "w/o category" : it->c_str());
      snprintf(buf, sizeof(buf) - 1,
               "%10.10s: %4.4f (%d/%d) %4.4f (%d/%d) %4.4f\n",
               category,
               pr,
               static_cast<int>(c),
               static_cast<int>(p),
               re,
               static_cast<int>(c),
               static_cast<int>(r),
               F);
      std::cout << buf;
    }

    return true;
  }

  static bool eval_dep(const char *answer_file,
                       const char *result_file) {
    std::ifstream ifs1(answer_file);
    std::ifstream ifs2(result_file);
    CHECK_DIE(ifs1) << "no such file or directory: " << answer_file;
    CHECK_DIE(ifs2) << "no such file or directory: " << result_file;

    std::string tree_str1;
    std::string tree_str2;
    Tree tree1;
    Tree tree2;

    size_t all_chunk2 = 0;
    size_t all_chunk1 = 0;
    size_t all_sentence = 0;
    size_t correct_chunk1 = 0;
    size_t correct_chunk2 = 0;
    size_t correct_sentence = 0;

    while (ifs1 && ifs2) {
      tree_str1.clear();
      tree_str2.clear();
      CHECK_DIE(read_sentence(&ifs1, &tree_str1,
                              INPUT_DEP));
      CHECK_DIE(read_sentence(&ifs2, &tree_str2,
                              INPUT_DEP));
      CHECK_DIE(tree1.read(tree_str1.c_str(), tree_str1.size(),
                           INPUT_DEP)) << "cannot parse sentence";
      CHECK_DIE(tree2.read(tree_str2.c_str(), tree_str2.size(),
                           INPUT_DEP)) << "cannot parse sentence";
      CHECK_DIE(tree1.size() == tree2.size())
          << "tree size is different";
      CHECK_DIE(tree1.chunk_size() == tree2.chunk_size())
          << "tree size is different";


      if (tree1.chunk_size() == 0) continue;

      for (int i = 0;
           i < static_cast<int>(tree1.chunk_size() - 1); ++i) {
        const Chunk *chunk1 = tree1.chunk(i);
        const Chunk *chunk2 = tree2.chunk(i);
        if (i < static_cast<int>(tree1.chunk_size() - 2)) {
          if (chunk1->link == chunk2->link) {
            ++correct_chunk2;
          }
          ++all_chunk2;
        }
        if (chunk1->link == chunk2->link) {
          ++correct_chunk1;
        }
        ++all_chunk1;
      }

      bool is_all_correct = true;
      for (size_t i = 0; i < tree1.chunk_size(); ++i) {
        if (tree1.chunk(i)->link != tree2.chunk(i)->link) {
          is_all_correct = false;
          break;
        }
      }
      if (is_all_correct) {
        ++correct_sentence;
      }
      ++all_sentence;
    }

    const float p1 = all_chunk1 == 0 ? 0.0 :
        100.0 * correct_chunk1 / all_chunk1;
    const float p2 = all_chunk2 == 0 ? 0.0 :
        100.0 * correct_chunk2 / all_chunk2;
    const float s1 = all_sentence == 0 ? 0.0 :
        100.0 * correct_sentence / all_sentence;

    char buf[8192];
    snprintf(buf, sizeof(buf) - 1,
             "dependency level1: %4.4f (%d/%d)\n"
             "dependency level2: %4.4f (%d/%d)\n"
             "sentence         : %4.4f (%d/%d)\n",
             p1, static_cast<int>(correct_chunk1),
             static_cast<int>(all_chunk1),
             p2, static_cast<int>(correct_chunk2),
             static_cast<int>(all_chunk2),
             s1, static_cast<int>(correct_sentence),
             static_cast<int>(all_sentence));
    std::cout << buf;

    return true;
  }
};
}

// exports
int cabocha_system_eval(int argc, char **argv) {
  return CaboCha::Eval::eval(argc, argv);
}

