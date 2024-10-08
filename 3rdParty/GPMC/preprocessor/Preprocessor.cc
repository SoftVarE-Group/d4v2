#include "preprocessor/Preprocessor.h"
#include "arjun/arjun.h"
#include "TestSolver.h"
#include "core/SolverTypes.h"
#include "lib_sharpsat_td/subsumer.hpp"

#include <unordered_map>
#include <unordered_set>

using std::swap;

using namespace PPMC;
using namespace Glucose1;
using namespace std;

// Identifier
void Identifier::identify(Lit l1, Lit l2) {
  if (cidx[toInt(l1)] == -1 && cidx[toInt(l2)] == -1) {
    cidx[toInt(l1)] = cidx[toInt(l2)] = eqc.size();
    eqc.push_back({l1, l2});
    cidx[toInt(~l1)] = cidx[toInt(~l2)] = eqc.size();
    eqc.push_back({~l1, ~l2});
    num_elem += 2;
  } else if (cidx[toInt(l1)] == -1) {
    eqc[cidx[toInt(l2)]].push_back(l1);
    eqc[cidx[toInt(~l2)]].push_back(~l1);
    cidx[toInt(l1)] = cidx[toInt(l2)];
    cidx[toInt(~l1)] = cidx[toInt(~l2)];

    if (var(eqc[cidx[toInt(l2)]][0]) > var(l1)) {
      vector<Lit> &eq = eqc[cidx[toInt(l2)]];
      std::swap(eq[0], eq.back());
      eq = eqc[cidx[toInt(~l2)]];
      std::swap(eq[0], eq.back());
    }
  } else if (cidx[toInt(l2)] == -1) {
    eqc[cidx[toInt(l1)]].push_back(l2);
    eqc[cidx[toInt(~l1)]].push_back(~l2);
    cidx[toInt(l2)] = cidx[toInt(l1)];
    cidx[toInt(~l2)] = cidx[toInt(~l1)];
  } else {
    if (cidx[toInt(l1)] == cidx[toInt(l2)])
      return;
    assert(cidx[toInt(l1)] != cidx[toInt(l2)]);

    Var d1 = var(eqc[cidx[toInt(l1)]][0]);
    Var d2 = var(eqc[cidx[toInt(l2)]][0]);

    if (d1 < d2) {
      MergeEquivClasses(cidx[toInt(l1)], cidx[toInt(l2)]);
      MergeEquivClasses(cidx[toInt(~l1)], cidx[toInt(~l2)]);
    } else {
      MergeEquivClasses(cidx[toInt(l2)], cidx[toInt(l1)]);
      MergeEquivClasses(cidx[toInt(~l2)], cidx[toInt(~l1)]);
    }
  }
}
void Identifier::removeEquivClass(Lit l) {
  if (cidx[toInt(l)] >= 0) {
    for (Lit s : {l, ~l}) {
      int idx = cidx[toInt(s)];
      for (Lit le : eqc[idx])
        cidx[toInt(le)] = -1;
      eqc[idx].clear();
    }
    num_elem -= 2;
  }
}
void Identifier::MergeEquivClasses(int c1, int c2) {
  for (Lit l : eqc[c2]) {
    eqc[c1].push_back(l);
    cidx[toInt(l)] = c1;
  }
  eqc[c2].clear();
  num_elem--;
}

// Preprocessor
template <class T_data>
bool Preprocessor<T_data>::Simplify(GPMC::Instance<T_data> *instance) {
  ins = instance;

  printCNFInfo("Init", true);

  if (ins->unsat || !SAT_FLE())
    return false;

  if (ins->vars > config.varlimit)
    return true;

  if (config.cs)
    Strengthen();

  int start_cls = ins->clauses.size();

  for (int i = 0; i < config.reps; i++) {
    int vars = ins->vars;
    int cls = ins->clauses.size();

    if (cpuTime() > config.timelim)
      break;

    if (config.ee)
      MergeAdjEquivs();
    if (config.ve) {
      if (ins->projected)
        VariableEliminate(false);
      if (config.ve_dve) {
        VariableEliminate(true);
      }
    }
    if (config.cs)
      Strengthen();

    if (cpuTime() > config.timelim ||
        ((vars == ins->vars) && (cls == ins->clauses.size())) ||
        (ins->clauses.size() > (double)1.1 * start_cls))
      break;
  }

  int learnts = ins->learnts.size();
  for (int i = ins->learnts.size() - 1; i >= 0; i--) {
    vector<Lit> &clause = ins->learnts[i];
    if (clause.size() > 3)
      sspp::SwapDel(ins->learnts, i);
  }
  if (cpuTime() < config.timelim)
    SAT_FLE();

  if (learnts * 3 / 2 < ins->learnts.size()) {
    if (ins->vars > 0) {
      int m = ins->clauses.size() / ins->vars;
      int len = std::max(m + 1, 3);
      for (int i = ins->learnts.size() - 1; i >= 0; i--) {
        vector<Lit> &clause = ins->learnts[i];
        if (clause.size() > len)
          sspp::SwapDel(ins->learnts, i);
      }
    }
  }

  printCNFInfo("Simp");

  ins = NULL;

  return true;
}

template <class T_data> bool Preprocessor<T_data>::SAT_FLE() {
  TestSolver S(ins->vars, ins->clauses, ins->learnts, ins->assignedLits);

  if (!S.okay())
    return false;

  if (!S.FailedLiterals() || S.Solve() == l_False) {
    ins->unsat = true;
    return false;
  }

  S.exportLearnts(ins->learnts);
  Compact(S.getAssigns());
  sspp::SortAndDedup(ins->clauses);
  sspp::SortAndDedup(ins->learnts);
  Subsume();

  if (config.verb >= 1)
    printCNFInfo("SAT_FLE");

  return true;
}

template <class T_data> bool Preprocessor<T_data>::Strengthen() {
  TestSolver S(ins->vars, ins->clauses, ins->learnts, ins->assignedLits);
  bool removecl = false;
  bool removelit = false;

  for (int i = ins->clauses.size() - 1; i >= 0; i--) {
    vec<Lit> assump;

    for (int j = ins->clauses[i].size() - 1; j >= 0; j--) {
      assump.clear();
      removelit = false;

      for (int k = 0; k < ins->clauses[i].size(); k++) {
        Lit l = ins->clauses[i][k];
        if (S.value(l) == l_True) {
          removecl = true;
          break;
        }
        if (k == j) {
          if (S.value(l) == l_False) {
            removelit = true;
            break;
          }
        } else if (S.value(l) == l_Undef) {
          assump.push(~l);
        }
      }
      if (removecl)
        break;

      if (removelit || S.falsifiedBy(assump)) {
        for (int k = j + 1; k < ins->clauses[i].size(); k++)
          ins->clauses[i][k - 1] = ins->clauses[i][k];
        ins->clauses[i].pop_back();

        if (ins->clauses[i].size() == 1) {
          S.assign(ins->clauses[i][0]);
          S.bcp(); // no conflict because SAT test was already passed.
          removecl = true;
          break;
        }
      }
    }
    if (removecl) {
      sspp::SwapDel(ins->clauses, i);
      removecl = false;
    }
  }

  S.exportLearnts(ins->learnts);
  Compact(S.getAssigns());
  sspp::SortAndDedup(ins->clauses);
  sspp::SortAndDedup(ins->learnts);
  Subsume();

  if (config.verb >= 1)
    printCNFInfo("ClStrg");

  return true;
}

template <class T_data> bool Preprocessor<T_data>::MergeAdjEquivs() {
  // This merges equivalent adjacent literals.
  // The equivalence check is lazy, using unit propagation not Sat solving.

  Identifier Id(ins->vars);
  {
    vector<sspp::Bitset> adjmat;
    adjmat.resize(ins->vars);
    for (int i = 0; i < ins->vars; i++)
      adjmat[i] = sspp::Bitset(ins->vars);

    for (const auto &cls : {ins->clauses, ins->learnts}) {
      for (const auto &clause : cls)
        for (int i = 0; i < clause.size(); i++)
          for (int j = i + 1; j < clause.size(); j++) {
            Var v1 = var(clause[i]);
            Var v2 = var(clause[j]);
            if (!adjmat[v1].Get(v2)) {
              adjmat[v1].SetTrue(v2);
              adjmat[v2].SetTrue(v1);
            }
          }
    }

    //
    // assert: any projected var idx < any non-projceted var idx
    //

    TestSolver S(ins->vars, ins->clauses, ins->learnts, ins->assignedLits);
    Glucose1::vec<Glucose1::Lit> &trail = S.getTrail();

    for (Var v1 = 0; v1 < ins->vars; v1++) {
      for (Var v2 = v1 + 1; v2 < ins->vars; v2++) {
        if (!adjmat[v1].Get(v2))
          continue;

        Lit l1 = mkLit(v1);
        Lit l2 = mkLit(v2);
        int idx1, idx2;

        if (S.value(l1) != l_Undef)
          break;
        if (S.value(l2) != l_Undef)
          continue;

        int pos = trail.size();

        if (S.falsifiedBy(l1, l2) && S.falsifiedBy(~l1, ~l2)) {
          Id.identify(l1, ~l2);
        } else if (S.falsifiedBy(l1, ~l2) && S.falsifiedBy(~l1, l2)) {
          Id.identify(l1, l2);
        }

        if (trail.size() - pos > 0) {
          int ppos = pos;
          int cpos = trail.size();

          while (ppos < cpos) {
            for (int i = ppos; i < cpos; i++) {
              int idx = Id.getIndex(trail[i]);
              if (idx == -1) {
                S.assign(trail[i]);
              } else {
                for (Lit l : Id.getEquivClasses()[idx])
                  S.assign(l);
              }
              S.bcp();

              Id.removeEquivClass(trail[i]);
            }
            ppos = cpos;
            cpos = trail.size();
          }
          for (int i = pos; i < trail.size(); i++)
            ins->assignedLits.push_back(trail[i]);
        }
      }
    }
  }

  bool subsump = false;
  if (Id.hasEquiv()) {
    vector<Lit> map(2 * ins->vars);
    int new_id = 0;
    for (int i = 0; i < ins->vars; i++) {
      Lit dl = Id.delegateLit(mkLit(i));
      Lit l = mkLit(i);

      if (mkLit(i) == dl) {
        Lit newlit = mkLit(new_id);
        map[toInt(l)] = newlit;
        map[toInt(~l)] = ~newlit;
        if (ins->weighted && i < ins->npvars) {
          ins->lit_weights[toInt(newlit)] = ins->lit_weights[toInt(l)];
          ins->lit_weights[toInt(~newlit)] = ins->lit_weights[toInt(~l)];
        }
        new_id++;
      } else {
        Lit newlit = map[toInt(dl)];
        map[toInt(l)] = newlit;
        map[toInt(~l)] = ~newlit;
        if (ins->weighted && i < ins->npvars) {
          ins->lit_weights[toInt(newlit)] *= ins->lit_weights[toInt(l)];
          ins->lit_weights[toInt(~newlit)] *= ins->lit_weights[toInt(~l)];
        }
      }
      if (i == ins->npvars - 1)
        ins->npvars = new_id;
    }
    ins->vars = new_id;
    ins->ispvars.clear();
    ins->ispvars.resize(ins->npvars, true);
    ins->ispvars.resize(ins->vars, false);

    if (ins->assignedLits.size() > 0) {
      vector<Lit> assignedLits_new;
      for (Lit l : ins->assignedLits)
        assignedLits_new.push_back(map[toInt(l)]);
      ins->assignedLits = assignedLits_new;
    }

    RewriteClauses(ins->clauses, map);
    RewriteClauses(ins->learnts, map);

    // update the variable map
    if (ins->keepVarMap) {
      for (int i = 0; i < ins->gmap.size(); i++) {
        Lit l = ins->gmap[i];
        if (l == lit_Undef)
          continue;
        ins->gmap[i] = map[toInt(l)];
      }
    }

    subsump = true;
  }

  if (ins->assignedLits.size() > 0) {
    sspp::SortAndDedup(ins->assignedLits);

    TestSolver S(ins->vars, ins->clauses, ins->learnts, ins->assignedLits);
    Compact(S.getAssigns());
    sspp::SortAndDedup(ins->clauses);
    sspp::SortAndDedup(ins->learnts);

    subsump = true;
  }

  if (subsump)
    Subsume();

  if (config.verb >= 1)
    printCNFInfo("EquivEl");
  return true;
}

template <class T_data> bool Preprocessor<T_data>::VariableEliminate(bool dve) {
  vector<int> vars;
  int origclssz = ins->clauses.size();
  int reps = dve ? config.dve_reps : config.ve_reps;
  int times = 0;
  while (times < reps) {
    vars.clear();
    if (dve)
      pickDefVars(vars);
    else
      pickVars(vars);

    if (vars.size() == 0)
      break;

    int lastidx = ElimVars(vars);

    vec<lbool> cassign;
    if (ins->assignedLits.size() > 0) {
      sspp::SortAndDedup(ins->assignedLits);
      TestSolver S(ins->vars, ins->clauses, ins->learnts, ins->assignedLits);
      S.getAssigns().copyTo(cassign);
    } else {
      cassign.growTo(ins->vars, l_Undef);
    }

    // Here we abuse Compact, assuming that deleted vars was assigned.
    // We can assume that the deleted vars are non-projected vars or the
    // counting mode is not a weighted one.
    assert(lastidx <= vars.size());
    vars.resize(lastidx);
    for (int i = 0; i < lastidx; i++) {
      Var v = vars[i];
      if (cassign[v] == l_Undef)
        cassign[v] = l_True; // just want to treat v as not a free variable and
                             // eliminate...
    }

    Compact(cassign, vars);
    Subsume();

    times++;
    if (times % 1000 == 0 && config.verb >= 1)
      printCNFInfo(dve ? "DefVE" : "VE");
    if (ins->clauses.size() > origclssz)
      break;
  }

  if (config.verb >= 1)
    printCNFInfo(dve ? "DefVE" : "VE");

  return true;
}

template <class T_data>
inline bool
Preprocessor<T_data>::isVECandidate(Graph &G, bool def, vector<int> &freq,
                                    std::vector<float> &cl_size, int i) const {
  return (G.isSimplical(i) &&
          min(freq[toInt(mkLit(i))], freq[toInt(~mkLit(i))]) <= 4) ||
         (config.ve_more && (freq[toInt(mkLit(i))] * freq[toInt(~mkLit(i))] <=
                             freq[toInt(mkLit(i))] + freq[toInt(~mkLit(i))]));
}

template <class T_data> void Preprocessor<T_data>::pickVars(vector<Var> &vars) {
  vars.clear();

  vector<int> freq;
  vector<float> cl_size;
  Graph G(ins->vars, ins->clauses, ins->learnts, freq, cl_size);

  for (int i = ins->npvars; i < ins->vars; i++) { // for only projected vars
    if (isVECandidate(G, false, freq, cl_size, i))
      vars.push_back(i);
  }

  sort(vars.begin(), vars.end(), [&](int a, int b) {
    int ca = freq[toInt(mkLit(a))] * freq[toInt(~mkLit(a))];
    int cb = freq[toInt(mkLit(b))] * freq[toInt(~mkLit(b))];
    if (ca == cb) {
      return cl_size[a] > cl_size[b];
    }
    return ca < cb;
  });
}

template <class T_data>
void Preprocessor<T_data>::pickDefVars(vector<Var> &vars) {
  vars.clear();
#if 0
  vector<int> map(ins->vars);
  vector<int> candv;

  double stime = cpuTime();
  vector<int> freq;
  {
    Graph G(ins->vars, ins->clauses, ins->learnts, freq);

    int count = 0;
    for (int i = 0; i < ins->npvars; i++) {
      map[i] = i;

      if (isVECandidate(G, true, freq, i)) {
        if (ins->weighted && ins->lit_weights[toInt(mkLit(i))] !=
                                 ins->lit_weights[toInt(~mkLit(i))])
          continue;

        map[i] = ((ins->vars << 1) - ins->npvars) + candv.size();
        candv.push_back(i);
      }
    }
  }

  if (candv.size() == 0)
    return;

  int newvars =
      ((ins->vars + candv.size()) << 1) -
      ins->npvars; // ins->vars + candv.size()*2 + ins->vars - ins->npvars;
  TestSolver S(newvars);
  for (const auto &clause : ins->clauses) {
    S.addClauseWith(clause); // add the existing clause

    bool toduplicate = false;
    for (Lit l : clause)
      if (var(l) >= ins->npvars || map[var(l)] >= ins->vars) {
        toduplicate = true;
        break;
      }

    if (toduplicate) {
      vector<Lit> newc;
      for (Lit l : clause) {
        Lit nl;
        if (var(l) >= ins->npvars)
          nl = mkLit((ins->vars - ins->npvars) + var(l), sign(l));
        else if (map[var(l)] >= ins->vars)
          nl = mkLit(map[var(l)], sign(l));
        else
          nl = l;
        assert(var(nl) < newvars);
        newc.push_back(nl);
      }
      S.addClauseWith(newc);
    }
  }
  for (int v : candv) {
    S.addClauseWith({~mkLit(map[v] + candv.size()), mkLit(v), ~mkLit(map[v])});
    S.addClauseWith({~mkLit(map[v] + candv.size()), ~mkLit(v), mkLit(map[v])});
  }

  vector<bool> def(ins->npvars, false);
  vec<Lit> assumptions;
  for (int v : candv) {
    assumptions.clear();
    assumptions.push(mkLit(v));
    assumptions.push(~mkLit(map[v]));
    for (int w : candv) {
      if (w != v && !def[w]) {
        assumptions.push(mkLit(map[w] + candv.size()));
      }
    }
    if (S.Solve(assumptions) == l_False) {
      def[v] = true;
      vars.push_back(v);
    }

    if (cpuTime() - stime > config.dve_timelimit)
      break;
  }
#endif

  ArjunNS::Arjun arjun;
  arjun.new_vars(ins->vars);
  for (auto cl : ins->clauses) {
    std::vector<CMSat::Lit> cl_conf;
    for (auto l : cl) {
      cl_conf.push_back(CMSat::Lit(var(l), sign(l)));
    }
    arjun.add_clause(cl_conf);
  }
  std::vector<uint32_t> sample;
  for (int i = 0; i < ins->npvars; i++) {
    sample.push_back(i);
  }
  arjun.set_sampl_vars(sample);
  std::vector<uint32_t> indpendent = arjun.run_backwards();
  std::unordered_set<uint32_t> set(indpendent.begin(), indpendent.end());

  vector<int> freq;
  vector<float> cl_size;
  Graph G(ins->vars, ins->clauses, ins->learnts, freq, cl_size);
  for (int i = 0; i < ins->npvars; i++) {
    if (set.find(i) == set.end() && isVECandidate(G, true, freq, cl_size, i)) {
      vars.push_back(i);
    }
  }
  sort(vars.begin(), vars.end(), [&](int a, int b) {
    int ca = freq[toInt(mkLit(a))] * freq[toInt(~mkLit(a))];
    int cb = freq[toInt(mkLit(b))] * freq[toInt(~mkLit(b))];
    if (ca == cb) {
      float ca = cl_size[toInt(mkLit(a))] + cl_size[toInt(~mkLit(a))];
      float cb = cl_size[toInt(mkLit(b))] + cl_size[toInt(~mkLit(b))];
      return ca > cb;
    }
    return ca < cb;
  });
}

template <class T_data>
int Preprocessor<T_data>::ElimVars(const vector<Var> &vars) {
  int idx = 0;
  int origclssz = ins->clauses.size();

  vector<int> deleted(ins->vars, false);
  for (; idx < vars.size(); idx++) {
    Var v = vars[idx];
    if (ins->clauses.size() > origclssz)
      break;

    if (config.ve_check) {
      int p = 0, n = 0;
      for (int i = ins->clauses.size() - 1; i >= 0; i--) {
        vector<Lit> &clause = ins->clauses[i];
        for (int j = 0; j < clause.size(); j++) {
          if (var(clause[j]) == v) {
            bool negative = sign(clause[j]);
            if (negative) {
              n++;
            } else {
              p++;
            }
            break;
          }
        }
      }
      if (!(min(p, n) <= 4 || p * n <= n + p)) {
        break;
      }
    }
    vector<vector<Lit>> pos;
    vector<vector<Lit>> neg;

    // find clauses with literals of v
    for (int i = ins->clauses.size() - 1; i >= 0; i--) {
      vector<Lit> &clause = ins->clauses[i];
      for (int j = 0; j < clause.size(); j++) {
        if (var(clause[j]) == v) {
          bool negative = sign(clause[j]);
          sspp::ShiftDel(clause, j);

          if (negative) {
            neg.push_back(clause);
          } else {
            pos.push_back(clause);
          }
          sspp::SwapDel(ins->clauses, i);
          break;
        }
      }
    }

    // var elimination by resolution
    for (const auto &c1 : pos) {
      for (const auto &c2 : neg) {
        vector<Lit> newc;
        bool taut = false;
        int i = 0;
        int j = 0;

        while (i < c1.size()) {
          while (j < c2.size() && var(c2[j]) < var(c1[i])) {
            newc.push_back(c2[j]);
            j++;
          }
          if (j < c2.size() && var(c2[j]) == var(c1[i])) {
            if (c2[j] == c1[i]) {
              newc.push_back(c2[j]);
              i++, j++;
              continue;
            } else {
              taut = true;
              break;
            }
          } else {
            newc.push_back(c1[i]);
            i++;
          }
        }
        while (j < c2.size()) {
          newc.push_back(c2[j]);
          j++;
        }
        if (!taut) {
          if (newc.size() == 1)
            ins->assignedLits.push_back(newc[0]);
          else
            ins->clauses.push_back(newc);
        }
      }
    }

    if (neg.size() == 0) {
      // MEMO: v is defined by others. Thus, v must be true on all models.
      ins->assignedLits.push_back(mkLit(v));
    } else if (pos.size() == 0) {
      ins->assignedLits.push_back(~mkLit(v));
    } else {
      deleted[v] = true;
    }
  }

  for (int i = ins->learnts.size() - 1; i >= 0; i--) {
    vector<Lit> &clause = ins->learnts[i];
    for (int j = 0; j < clause.size(); j++) {
      if (deleted[var(clause[j])]) {
        sspp::SwapDel(ins->learnts, i);
        break;
      }
    }
  }
  return idx;
}

static inline lbool val(const vec<lbool> &assigns, Lit p) {
  return assigns[var(p)] ^ sign(p);
}

template <class T_data>
void Preprocessor<T_data>::Compact(const vec<lbool> &assigns,
                                   const vector<Var> &elimvars) {
  int varnum = 0;
  vector<bool> occurred;
  occurred.resize(ins->vars, false);

  // Compact Clauses
  CompactClauses(assigns, ins->clauses, occurred, varnum);
  CompactClauses(assigns, ins->learnts, occurred, varnum);

  // Compact Variables
  int new_idx = 0;
  vector<Var> map;
  vector<Var> nonpvars;

  map.clear();
  map.resize(ins->vars, var_Undef);
  if (ins->keepVarMap)
    for (auto v : elimvars)
      if (v < ins->npvars)
        map[v] = GPMC::var_Determined;

  nonpvars.reserve(ins->vars - ins->npvars);

  vector<T_data> lit_weights2;
  if (ins->weighted) {
    lit_weights2 = ins->lit_weights;
    ins->lit_weights.resize(varnum * 2);
  }

  for (Var v = 0; v < ins->vars; v++) {
    if (occurred[v]) {
      if (ins->ispvars[v]) {
        map[v] = new_idx;
        if (ins->weighted) {
          ins->lit_weights[toInt(mkLit(new_idx))] =
              lit_weights2[toInt(mkLit(v))];
          ins->lit_weights[toInt(~mkLit(new_idx))] =
              lit_weights2[toInt(~mkLit(v))];
        }
        new_idx++;
      } else
        nonpvars.push_back(v);
    } else {
      if (ins->ispvars[v]) {
        if (assigns[v] == l_Undef) {
          ins->freevars++;
          if (ins->weighted)
            ins->gweight *= lit_weights2[toInt(mkLit(v, true))] +
                            lit_weights2[toInt(mkLit(v, false))];
        } else {
          if (ins->weighted)
            ins->gweight *=
                lit_weights2[toInt(mkLit(v, assigns[v] == l_False))];
        }
      }
    }
  }

  ins->npvars = new_idx;
  for (int i = 0; i < nonpvars.size(); i++) {
    map[nonpvars[i]] = new_idx;
    new_idx++;
  }
  ins->vars = new_idx;

  ins->ispvars.clear();
  ins->ispvars.resize(ins->npvars, true);
  ins->ispvars.resize(ins->vars, false);

  // Replace literals according to map
  RewriteClauses(ins->clauses, map);
  RewriteClauses(ins->learnts, map);

  // update the variable map
  if (ins->keepVarMap) {
    std::unordered_map<Glucose1::Var, int> table;

    for (int i = 0; i < ins->gmap.size(); i++) {
      Lit l = ins->gmap[i];
      if (l == lit_Undef)
        continue;

      Var x = var(ins->gmap[i]);
      if (assigns[x] == l_Undef) {
        Var newv = map[var(l)];
        if (newv >= 0) {
          ins->gmap[i] = mkLit(newv, sign(l));
        } else {
          assert(newv == var_Undef);
          auto itr = table.find(x);
          if (itr == table.end()) {
            table[x] = ins->freeLitClasses.size();
            ins->freeLitClasses.push_back({mkLit(i, sign(l))});
          } else {
            ins->freeLitClasses[itr->second].push_back(mkLit(i, sign(l)));
          }
          ins->gmap[i] = lit_Undef;
        }
      } else if (map[var(l)] == GPMC::var_Determined) {
        ins->definedVars.push_back(i);
        ins->gmap[i] = lit_Undef;
      } else {
        bool lsign = ((assigns[x] == l_False) != sign(l));
        ins->fixedLits.push_back(mkLit(i, lsign));
        ins->gmap[i] = lit_Undef;
      }
    }
  }

  ins->assignedLits.clear();
}

template <class T_data>
void Preprocessor<T_data>::CompactClauses(const vec<lbool> &assigns,
                                          vector<vector<Lit>> &cls,
                                          vector<bool> &occurred, int &varnum) {
  int i1, i2;
  int j1, j2;

  for (i1 = 0, i2 = 0; i1 < cls.size(); i1++) {
    vector<Lit> &c = cls[i1];
    for (j1 = 0, j2 = 0; j1 < c.size(); j1++) {
      if (val(assigns, c[j1]) == l_Undef)
        c[j2++] = c[j1];
      else if (val(assigns, c[j1]) == l_True) {
        goto NEXTC;
      }
    }
    c.resize(j2);
    assert(c.size() == j2);
    assert(c.size() > 1);

    for (auto l : c) {
      Var v = var(l);
      if (!occurred[v]) {
        occurred[v] = true;
        varnum++;
      }
    }

    cls[i2++] = cls[i1];
  NEXTC:;
  }
  cls.resize(i2);
}

template <class T_data>
void Preprocessor<T_data>::RewriteClauses(vector<vector<Lit>> &cls,
                                          const vector<Var> &map) {
  // We assume that map is injective for active variables, i.e., this does not
  // change the length of clauses.

  for (int i = 0; i < cls.size(); i++) {
    vector<Lit> &c = cls[i];
    for (int j = 0; j < c.size(); j++)
      c[j] = mkLit(map[var(c[j])], sign(c[j]));
    sort(c.begin(), c.end());
  }
}

template <class T_data>
void Preprocessor<T_data>::RewriteClauses(vector<vector<Lit>> &cls,
                                          const vector<Lit> &map) {
  // map may not be injective, i.e., this may strengthen clauses.

  for (int i = 0; i < cls.size(); i++) {
    vector<Lit> &c = cls[i];
    for (int j = 0; j < c.size(); j++)
      c[j] = map[toInt(c[j])];

    sspp::SortAndDedup(c);

    bool unit = false;
    bool taut = false;
    if (c.size() == 1) {
      ins->assigns[var(c[0])] = sign(c[0]) ? l_False : l_True;
      ins->assignedLits.push_back(c[0]);
      unit = true;
    } else {
      for (int j = 1; j < c.size(); j++)
        if (var(c[j]) == var(c[j - 1])) {
          taut = true;
          break;
        }
    }
    if (unit || taut) {
      sspp::SwapDel(cls, i);
      i--;
    }
  }
}

template <class T_data> void Preprocessor<T_data>::Subsume() {
  {
    sspp::Subsumer subsumer;
    ins->clauses = subsumer.Subsume(ins->clauses);
    sspp::SortAndDedup(ins->clauses);
  }

  if (ins->learnts.empty())
    return;
  for (const auto &clause : ins->clauses) {
    ins->learnts.push_back(clause);
  }

  {
    sspp::Subsumer subsumer_lrnt;
    ins->learnts = subsumer_lrnt.Subsume(ins->learnts);
    sspp::SortAndDedup(ins->learnts);
  }

  for (int i = 0; i < ins->learnts.size(); i++) {
    if (std::binary_search(ins->clauses.begin(), ins->clauses.end(),
                           ins->learnts[i])) {
      sspp::SwapDel(ins->learnts, i);
      i--;
    }
  }
}

template class PPMC::Preprocessor<mpz_class>;
