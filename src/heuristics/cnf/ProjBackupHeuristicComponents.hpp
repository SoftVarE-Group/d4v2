#pragma once
#include "PartitioningHeuristicStatic.hpp"
#include "src/heuristics/ProjBackupHeuristic.hpp"
#include "src/hyperGraph/HyperGraphExtractor.hpp"
#include "src/partitioner/PartitionerManager.hpp"
#include "src/solvers/WrapperSolver.hpp"
#include "src/specs/cnf/SpecManagerCnf.hpp"
#include "src/utils/EquivExtractor.hpp"
#include "src/utils/UnionFind.hpp"
#include <unordered_set>
namespace d4 {
struct NProjComponent{
    std::unordered_set<int> neigh;
    std::unordered_set<int> ccover;
    int nproj_cnt= 0;
};
class ProjBackupHeuristicComponents : public ProjBackupHeuristic {
  SpecManagerCnf &m_om;
  std::vector<bool> m_current_component;
  UnionFind m_uf;
  double m_min_ccover;
  int m_scroing_method;
  int calls=0;


public:
  ProjBackupHeuristicComponents(Config &config, SpecManager &om,
                                WrapperSolver &s, std::ostream &out);
  ~ProjBackupHeuristicComponents(){
      std::cout<<"Calls to BackupH "<<calls<<std::endl;
  }
  bool computeCutSetDyn(ProjVars &component, std::vector<Var> &cutset) final;
};
} // namespace d4
