#pragma once
#include "PartitioningHeuristicStatic.hpp"
#include "src/hyperGraph/HyperGraphExtractor.hpp"
#include "src/partitioner/PartitionerManager.hpp"
#include "src/solvers/WrapperSolver.hpp"
#include "src/specs/cnf/SpecManagerCnf.hpp"
#include "src/utils/EquivExtractor.hpp"
#include "src/heuristics/ProjBackupHeuristic.hpp"
namespace d4 {
class ProjBackupHeuristicHypergraph:public ProjBackupHeuristic {
private:
  SpecManagerCnf &m_om;
  WrapperSolver &m_s;
  EquivExtractor m_em;
  PartitionerManager *m_pm;

  unsigned m_nbVar, m_nbClause;
  unsigned m_counter = 0;
  bool m_equivSimp;
  bool m_reduceFormula;
  bool m_useEquiv;

  // to store the hypergraph, and then avoid reallocated memory.
  HyperGraph m_hypergraph;
  HyperGraphExtractor *m_hypergraphExtractor;

  std::vector<bool> m_markedVar;
  std::vector<int> m_partition;
  std::vector<Var> m_equivClass;

  void computeEquivClass(std::vector<Var> &component,
                         std::vector<Lit> &unitEquiv,
                         std::vector<Var> &equivClass,
                         std::vector<std::vector<Var>> &equivVar);

public:
  ProjBackupHeuristicHypergraph(Config &config, SpecManager &om, WrapperSolver &s,
                      std::ostream &out);
  virtual ~ProjBackupHeuristicHypergraph();

  bool computeCutSetDyn(ProjVars &component, std::vector<Var> &cutset) final;
};
} // namespace d4
