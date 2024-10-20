#pragma once
#include <vector>

#include "src/preprocs/PreprocManager.hpp"
#include "3rdParty/glucose-3.0/core/Solver.h"
#include "src/problem/cnf/ProblemManagerCnf.hpp"



namespace d4{
// Use GPMCs preproc in D4
class PreprocGPMC:public PreprocManager{

  WrapperSolver *ws;

public: 
  PreprocGPMC(Config &config, std::ostream &out);
  virtual ~PreprocGPMC();
  
  virtual ProblemManager *run(ProblemManager *pin,
                              LastBreathPreproc &lastBreath) override;

};
}




namespace gpmc {





}
