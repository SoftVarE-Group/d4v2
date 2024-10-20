#pragma once
#include "3rdParty/glucose-3.0/core/Solver.h"
#include "3rdParty/glucose-3.0/core/SolverTypes.h"
#include "3rdParty/glucose-3.0/mtl/Vec.h"
#include "lib_sharpsat_td/bitset.hpp"

#include <vector>
// This code was stolen straight from gpmc
namespace PRE {
class Graph {
public:
  Graph() : nodes(0), edges(0) {}
  Graph(int vars, const std::vector<std::vector<Glucose::Lit>> &clauses);
  Graph(int vars, const std::vector<std::vector<Glucose::Lit>> &clauses,
        const std::vector<std::vector<Glucose::Lit>> &learnts,
        std::vector<int> &freq);

  Graph(int vars, const std::vector<std::vector<Glucose::Lit>> &clauses,
        const std::vector<std::vector<Glucose::Lit>> &learnts,
        std::vector<int> &freq, std::vector<double> &cl_size);

  void init(int n);
  void clear();

  int numNodes() { return this->nodes; }
  int numEdges() { return this->edges; }

  void addEdge(int v1, int v2);
  bool hasEdge(int v1, int v2) { return adj_mat[v1].Get(v2); }
  const std::vector<int> Neighbors(int v) const { return adj_list[v]; }

  bool isSimplical(int v, int proj) {
    bool clean = true;
    for (auto v : adj_list[v]) {
      clean &= v >= proj;
    }
    if (clean) {
      return true;
    }

    return isClique(adj_list[v]);
  }

  bool isSimplical(int v) { return isClique(adj_list[v]); }
  bool isClique(const std::vector<int> &adj);

  // Debug
  void toDimacs(std::ostream &out, bool withHeader = true);

protected:
  int nodes;
  int edges;
  std::vector<std::vector<int>> adj_list;
  std::vector<sharpsat_td::Bitset> adj_mat;
};
class Identifier {
public:
  Identifier(int vars) {
    cidx.resize(2 * vars, -1);
    num_elem = 0;
  }

  void identify(Glucose::Lit l1, Glucose::Lit l2);

  bool hasEquiv() { return num_elem > 0; }
  std::vector<std::vector<Glucose::Lit>> &getEquivClasses() { return eqc; }
  Glucose::Lit delegateLit(Glucose::Lit l) {
    return (cidx[toInt(l)] == -1) ? l : eqc[cidx[toInt(l)]][0];
  }
  int getIndex(Glucose::Lit l) { return cidx[toInt(l)]; }
  void removeEquivClass(Glucose::Lit l);

private:
  void MergeEquivClasses(int c1, int c2);

  std::vector<std::vector<Glucose::Lit>> eqc;
  std::vector<int> cidx;
  int num_elem;
};
class TestSolver : public Glucose::Solver {
public:
  TestSolver(int nvars) { newVars(nvars); }
  TestSolver(int nvars, std::vector<std::vector<Glucose::Lit>> clauses,
             std::vector<std::vector<Glucose::Lit>> learnts,
             std::vector<Glucose::Lit> assignedLits);

  void addClauseWith(const std::vector<Glucose::Lit> &ps, bool learnt = false);
  void resetClauses(std::vector<std::vector<Glucose::Lit>> &clauses);

  Glucose::lbool Solve() { return solve_(); }
  Glucose::lbool Solve(const Glucose::vec<Glucose::Lit> &assumptions) {
    budgetOff();
    setConfBudget(clauses.size() * 10);
    return solveLimited(assumptions);
  }

  bool falsifiedBy(Glucose::Lit l);
  bool falsifiedBy(Glucose::Lit l1, Glucose::Lit l2);
  bool falsifiedBy(Glucose::vec<Glucose::Lit> &assump);
  bool FailedLiterals();

  void exportLearnts(std::vector<std::vector<Glucose::Lit>> &learnts);

  void assign(Glucose::Lit l) { enqueue(l); }
  bool bcp() { return propagate() == Glucose::CRef_Undef; }

  Glucose::vec<Glucose::lbool> &getAssigns() { return assigns; }
  Glucose::vec<Glucose::Lit> &getTrail() { return trail; }

private:
  void newVars(int nvars);
  void backTo(int pos);
};
} // namespace PPMC
