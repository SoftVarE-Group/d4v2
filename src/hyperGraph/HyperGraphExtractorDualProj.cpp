
#include "HyperGraphExtractorDualProj.hpp"
namespace d4 {

HyperGraphExtractorDualProj::HyperGraphExtractorDualProj(unsigned nbVar,
                                                         unsigned nbClause,
                                                         SpecManagerCnf &specs,
                                                         int nproj_cost)
    : HyperGraphExtractorDual(nbVar, nbClause), m_specs(specs),
      m_nproj_cost(nproj_cost) {}

void HyperGraphExtractorDualProj::compute_stats(SpecManagerCnf &specs) {
  m_cost.resize(specs.getNbVariable() + 1);
  std::vector<int> occurence_cnt(specs.getNbVariable() + 1);
  double occurence_acc = 0;
  for (Var v = 1; v <= specs.getNbVariable(); v++) {
    if (!specs.isSelected(v)) {
      break;
    }
    occurence_cnt[v] = specs.getNbOccurrence(Lit::makeLitTrue(v)) +
                       specs.getNbOccurrence(Lit::makeLitFalse(v));
    occurence_acc += occurence_cnt[v];
  }
  double avg_occ = occurence_acc / specs.getNbVariable();
  double occ_var = 0;
  int max_occ = 0;
  for (Var v = 1; v <= specs.getNbVariable(); v++) {
    if (!specs.isSelected(v)) {
      break;
    }
    double diff = occurence_cnt[v] - avg_occ;
    occ_var += diff * diff;
    if (occurence_cnt[v] > max_occ) {
      max_occ = occurence_cnt[v];
    }
  }
  occ_var = sqrt(occ_var / specs.getNbVariable());

  int cost_nproj = 10;

  std::cout << "Avg " << avg_occ << std::endl;
  std::cout << "Max " << max_occ << std::endl;
  std::cout << "Var " << occ_var << std::endl;
  std::cout << "NProj Cost: " << cost_nproj << std::endl;
  for (Var v = 1; v <= specs.getNbVariable(); v++) {
    if (specs.isSelected(v)) {
      m_cost[v] = 1;
    } else {
      m_cost[v] = cost_nproj;
    }
  }
}
void HyperGraphExtractorDualProj::constructHyperGraph(
    SpecManagerCnf &om, std::vector<Var> &component,
    std::vector<Var> &equivClass, std::vector<std::vector<Var>> &equivVar,
    bool reduceFormula, std::vector<Var> &considered, HyperGraph &hypergraph) {
  unsigned pos = 0;
  m_idxClauses.resize(0);
  hypergraph.setSize(0);
  // std::cout << "Cost " << m_nProjCost << std::endl;

  // first considere the equivalence.

  for (auto &vec : equivVar) {
    unsigned &size = hypergraph[pos++];
    size = 0;
    int cost = m_nproj_cost;

    for (auto &v : vec) {
      if (om.varIsAssigned(v))
        continue;

      cost = std::min(cost, m_specs.isSelected(v) ? 1 : m_nproj_cost);
      assert(!m_markedVar[v]);
      for (auto l : {Lit::makeLitFalse(v), Lit::makeLitTrue(v)}) {
        IteratorIdxClause listIdx = om.getVecIdxClauseNotBin(l);
        for (int *ptr = listIdx.start; ptr != listIdx.end; ptr++) {
          int idx = *ptr;
          if (!m_markedClauses[idx]) {
            m_markedClauses[idx] = true;
            m_unmarkSet.push_back(idx);
            hypergraph[pos++] = idx;
            size++;
          }
        }

        listIdx = om.getVecIdxClauseBin(l);
        for (int *ptr = listIdx.start; ptr != listIdx.end; ptr++) {
          int idx = *ptr;
          if (!m_markedClauses[idx]) {
            m_markedClauses[idx] = true;
            m_unmarkSet.push_back(idx);
            hypergraph[pos++] = idx;
            size++;
          }
        }
      }
      m_markedVar[v] = true;
    }

    for (auto &idx : m_unmarkSet) {
      if (!m_keepClause[idx]) {
        m_keepClause[idx] = true;
        m_idxClauses.push_back(idx);
      }
      m_markedClauses[idx] = false;
    }
    m_unmarkSet.resize(0);
    assert(equivClass[vec.back()] == vec.back());

    if (!size)
      pos--;
    else {

      hypergraph.getCost()[hypergraph.getSize()] = cost;
      hypergraph.incSize();
      considered.push_back(vec.back());
    }
  }

  // next consider the remaining variables (unmarked).
  for (auto &v : component) {
    if (m_markedVar[v] || om.varIsAssigned(v))
      continue;
    m_markedVar[v] = true;

    unsigned &size = hypergraph[pos++];
    size = 0;

    for (auto l : {Lit::makeLitFalse(v), Lit::makeLitTrue(v)}) {
      IteratorIdxClause listIdx = om.getVecIdxClauseNotBin(l);
      for (int *ptr = listIdx.start; ptr != listIdx.end; ptr++) {
        int idx = *ptr;
        if (!m_keepClause[idx]) {
          m_keepClause[idx] = true;
          m_idxClauses.push_back(idx);
        }
        hypergraph[pos++] = idx;
        size++;
      }

      listIdx = om.getVecIdxClauseBin(l);
      for (int *ptr = listIdx.start; ptr != listIdx.end; ptr++) {
        int idx = *ptr;
        if (!m_keepClause[idx]) {
          m_keepClause[idx] = true;
          m_idxClauses.push_back(idx);
        }
        hypergraph[pos++] = idx;
        size++;
      }
    }

    if (!size)
      pos--;
    else {
      hypergraph.getCost()[hypergraph.getSize()] = m_specs.isSelected(v) ? 1 : m_nproj_cost;
      hypergraph.incSize();
      considered.push_back(v);
    }
  }

  // unmark.
  for (auto &v : component)
    m_markedVar[v] = false;

  // remove useless edges.
  if (reduceFormula)
    reduceHyperGraph(om, hypergraph, considered, m_idxClauses, equivClass);

  // unmark.
  for (auto &idx : m_idxClauses)
    m_keepClause[idx] = false;
}
} // namespace d4
