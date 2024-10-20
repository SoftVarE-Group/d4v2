#include "ProjBackupHeuristicComponents.hpp"
#include "src/hyperGraph/HyperGraphExtractorDualProj.hpp"
#include "src/utils/UnionFind.hpp"
#include <unordered_set>

namespace d4 {
ProjBackupHeuristicComponents::ProjBackupHeuristicComponents(
    Config &config, SpecManager &om, WrapperSolver &s, std::ostream &out)
    : m_om(dynamic_cast<SpecManagerCnf &>(om)),
      m_current_component(m_om.getNbVariable() + 1),
      m_uf(m_om.getNbVariable() + 1) {

  // FIXME: was not in original options, what shoud be the default values?
  m_min_ccover = 1.0; // vm["proj-backup-min-cover"].as<double>();
  m_scroing_method = 1; //vm["proj-backup-scoring-method"].as<int>();

} // constructor

bool ProjBackupHeuristicComponents::computeCutSetDyn(ProjVars &component,
                                                     std::vector<Var> &cutSet) {

  if (component.vars.size()/double(m_om.getNbVariable())<0.3||calls>20) {
    return false;
  }
  calls++;

  m_uf.clear();
  for (auto v : component.vars) {
    m_current_component[v] = true;
  }
  std::vector<int> nproj_clauses;
  std::vector<int> clauses;
  double clause_cnt = 0;
  for (int i = 0; i < m_om.getNbClause(); i++) {
    if (!m_om.isNotSatisfiedClauseAndInComponent(i, m_current_component)) {
      continue;
    }
    clauses.push_back(i);
    int set = -1;
    clause_cnt++;
    for (auto l : m_om.getClause(i)) {
      if (m_om.isSelected(l.var()) || m_om.litIsAssigned(l))
        continue;
      if (set == -1) {
        set = m_uf.find_set(l.var());
        nproj_clauses.push_back(i);
      } else {
        m_uf.union_sets(l.var(), set);
      }
    }
  }
  for (auto v : component.vars) {
    m_current_component[v] = false;
  }
  std::vector<int> set2index(m_om.getNbVariable() + 1, -1);
  std::vector<NProjComponent> nproj_sets;
  for (auto i : nproj_clauses) {
    int set = -1;
    for (auto l : m_om.getClause(i)) {
      if (m_om.isSelected(l.var()) || m_om.litIsAssigned(l))
        continue;
      set = m_uf.find_set(m_om.getClause(i).back().var());
    }
    if (set2index[set] == -1) {
      set2index[set] = nproj_sets.size();
      nproj_sets.push_back(NProjComponent{});
    }
    int idx = set2index[set];
    auto &inst = nproj_sets[idx];
    inst.ccover.insert(i);
    inst.nproj_cnt += 1;
    for (auto l : m_om.getClause(i)) {
      if (m_om.litIsAssigned(l)) {
        continue;
      }
      if (m_om.isSelected(l.var())) {
        inst.neigh.insert(l.var());
      } else {
        break;
      }
    }
  }
  if (nproj_sets.size() == 1) {
    return false;
  }
  // std::cout << "Found " << nproj_sets.size() << " components" << std::endl;
  for (auto &set : nproj_sets) {
    for (auto n : set.neigh) {
      Lit l = Lit::makeLitTrue(n);
      for (int i = 0; i < 2; i++) {
        auto iter = m_om.getVecIdxClause(l);
        for (int *cli = iter.start; cli != iter.end; cli++) {
          set.ccover.insert(*cli);
        }
        l = ~l;
      }
    }
  }
  int best_set = -1;
  double best_score = 0;
  for (int i = 0; i < nproj_sets.size(); i++) {
    auto &set = nproj_sets[i];
    double size = set.ccover.size();
    double cover = set.ccover.size() / clause_cnt;
    if (cover < m_min_ccover) {
      continue;
    }
    double cover_nproj = double(set.nproj_cnt) / set.ccover.size();
    double cover_proj =
        double(set.ccover.size() - set.nproj_cnt) / set.ccover.size();
    double score = 0;

    
    double cut_size = set.neigh.size() / double(component.nbProj);
    switch(m_scroing_method){
        case 0:
            score = cover * (1 - cut_size);
        case 1: 
            score = cover/cut_size;
    }
    assert(set.neigh.size() > 0);

    /*std::cout << "Component: Cut: " << cut_size << " Cover: " << cover
              << " BalanaceProj: " << cover_proj
              << " BalanaceNProj: " << cover_nproj<<"Score: "<<score <<
       std::endl;
    */
    if (score > best_score) {

      best_score = score;
      best_set = i;
    }
  }
  if (best_set == -1) {
    return false;

  } else {
    auto &neigh = nproj_sets[best_set].neigh;
    cutSet = std::vector<Var>(neigh.begin(), neigh.end());
    return true;
  }
}
} // namespace d4
