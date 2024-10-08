#include <algorithm>

#pragma once

#include <vector>

#include "../PreprocManager.hpp"
#include "src/solvers/WrapperSolver.hpp"
#include "util/PreprocGPMC.hpp"
// Preprocessor based on gpmc and sharpsat-td with
// custom partial resolution as explained in the thesis
namespace d4 {
class PreprocProj : public PreprocManager {
private:
  WrapperSolver *ws;
  PRE::ConfigPreprocessor config;
  bool keep_map;

public:
  PreprocProj(Config &config, std::ostream &out);
  ~PreprocProj();
  virtual ProblemManager *run(ProblemManager *pin,
                              LastBreathPreproc &lastBreath) override;
};
} // namespace d4
