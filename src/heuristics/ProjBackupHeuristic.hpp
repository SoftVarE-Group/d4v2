#pragma once
#include "src/solvers/WrapperSolver.hpp"
#include "src/specs/cnf/SpecManagerCnf.hpp"
#include <memory>
namespace d4 {
//TODO delete this since its useless
class ProjBackupHeuristic {
public:
  virtual ~ProjBackupHeuristic() {}
  virtual bool computeCutSetDyn(ProjVars &component, std::vector<Var> &cutset);
  static std::unique_ptr<ProjBackupHeuristic> make(Config &config,
                                                   SpecManager &p,
                                                   WrapperSolver &s,
                                                   std::ostream &out);
};
} // namespace d4
