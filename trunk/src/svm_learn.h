// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: svm_learn.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#include <vector>

namespace CaboCha {

class SVMModel;

class SVMSolver {
 public:
  static SVMModel *learn(const char *training_file,
                         const char *prev_model_file,
                         double C);
};
}  // namespace
