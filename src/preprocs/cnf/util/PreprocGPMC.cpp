#include "PreprocGPMC.hpp"
#include "arjun/arjun.h"
#include "3rdParty/glucose-3.0/utils/System.h"
#include "lib_sharpsat_td/subsumer.hpp"

#include <boost/multiprecision/gmp.hpp>
#include <unordered_map>
#include <unordered_set>



using namespace PRE;
using namespace Glucose;
using namespace std;

template <class T_data>
inline Glucose::lbool Instance<T_data>::value(Glucose::Var x) const {
  return assigns[x];
}
template <class T_data>
inline Glucose::lbool Instance<T_data>::value(Glucose::Lit p) const {
  return assigns[var(p)] ^ sign(p);
}

template <class T_data>
Instance<T_data>::Instance()
    : vars(0), weighted(false), projected(false), npvars(0), freevars(0),
      gweight(1), keepVarMap(false), unsat(false) {}

template <class T_data>
bool Instance<T_data>::addClause(vector<Lit> &ps, bool learnt) {
  sort(ps.begin(), ps.end());

  vec<Lit> oc;
  oc.clear();

  Lit p;
  int i, j, flag = 0;

  for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
    if (value(ps[i]) == l_True || ps[i] == ~p)
      return true;
    else if (value(ps[i]) != l_False && ps[i] != p)
      ps[j++] = p = ps[i];
  ps.resize(j);

  if (ps.size() == 0) {
    unsat = true;
    return false;
  } else if (ps.size() == 1) {
    assigns[var(ps[0])] = lbool(!sign(p));
    assignedLits.push_back(ps[0]);
    // No check by UP here. Preprocessor will do later.
  } else {
    clauses.push_back({});
    copy(ps.begin(), ps.end(), back_inserter(clauses.back()));
  }
  return true;
}

const Var var_Determined = {-2};

// Preprocessor
template <class T_data>
bool Preprocessor<T_data>::Simplify(Instance<T_data> *instance) {
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
      sharpsat_td::SwapDel(ins->learnts, i);
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
          sharpsat_td::SwapDel(ins->learnts, i);
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
  sharpsat_td::SortAndDedup(ins->clauses);
  sharpsat_td::SortAndDedup(ins->learnts);
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
      sharpsat_td::SwapDel(ins->clauses, i);
      removecl = false;
    }
  }

  S.exportLearnts(ins->learnts);
  Compact(S.getAssigns());
  sharpsat_td::SortAndDedup(ins->clauses);
  sharpsat_td::SortAndDedup(ins->learnts);
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
    vector<sharpsat_td::Bitset> adjmat;
    adjmat.resize(ins->vars);
    for (int i = 0; i < ins->vars; i++)
      adjmat[i] = sharpsat_td::Bitset(ins->vars);

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
    Glucose::vec<Glucose::Lit> &trail = S.getTrail();

    for (Var v1 = 0; v1 < ins->vars; v1++) {
      for (Var v2 = v1 + 1; v2 < ins->vars; v2++) {
        if (!adjmat[v1].Get(v2))
          continue;

        Lit l1 = mkLit(v1);
        Lit l2 = mkLit(v2);

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
    sharpsat_td::SortAndDedup(ins->assignedLits);

    TestSolver S(ins->vars, ins->clauses, ins->learnts, ins->assignedLits);
    Compact(S.getAssigns());
    sharpsat_td::SortAndDedup(ins->clauses);
    sharpsat_td::SortAndDedup(ins->learnts);

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
      sharpsat_td::SortAndDedup(ins->assignedLits);
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
                                    std::vector<double> &cl_size, int i) const {
  return (((!def&& !config.ve_only_simpical) || G.isSimplical(i)) &&
          min(freq[toInt(mkLit(i))], freq[toInt(~mkLit(i))]) <=
              config.ve_limit) ||
         (config.ve_more && (freq[toInt(mkLit(i))] * freq[toInt(~mkLit(i))] <=
                             freq[toInt(mkLit(i))] + freq[toInt(~mkLit(i))]));
}

template <class T_data> void Preprocessor<T_data>::pickVars(vector<Var> &vars) {
  vars.clear();

  vector<int> freq;
  vector<double> cl_size;
  vector<bool> is_simpical(ins->vars);
  Graph G(ins->vars, ins->clauses, ins->learnts, freq, cl_size);

  for (int i = ins->npvars; i < ins->vars; i++) { // for only projected vars
    if (isVECandidate(G, false, freq, cl_size, i)) {
      vars.push_back(i);
      if (config.ve_prefer_simpical) {
        is_simpical[i] = G.isSimplical(i);
      }
    }
  }
  sort(vars.begin(), vars.end(), [&](int a, int b) {
    int ca = freq[toInt(mkLit(a))] * freq[toInt(~mkLit(a))];
    int cb = freq[toInt(mkLit(b))] * freq[toInt(~mkLit(b))];
    if (config.ve_prefer_simpical) {
      if ((ca == 0) ^ (cb == 0)) {
        return ca < cb;
      }
      if (is_simpical[a] ^ is_simpical[b]) {
        return is_simpical[a] > is_simpical[b];
      }
    }
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
  vector<double> cl_size;
  vector<bool> is_simpical(ins->vars);
  Graph G(ins->vars, ins->clauses, ins->learnts, freq, cl_size);

  for (int i = 0; i < ins->npvars; i++) {
    if (set.find(i) == set.end() && isVECandidate(G,false,freq,cl_size,i)) {
      vars.push_back(i);
      is_simpical[i] = G.isSimplical(i);
    }
  }
  sort(vars.begin(), vars.end(), [&](int a, int b) {
    int ca = freq[toInt(mkLit(a))] * freq[toInt(~mkLit(a))];
    int cb = freq[toInt(mkLit(b))] * freq[toInt(~mkLit(b))];
    if (config.ve_prefer_simpical) {
      if ((ca == 0) ^ (cb == 0)) {
        return ca < cb;
      }
      if (is_simpical[a] ^ is_simpical[b]) {
        return is_simpical[a] > is_simpical[b];
      }
    }
    if (ca == cb) {
      return cl_size[a] > cl_size[b];
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
      if (!(min(p, n) <= config.ve_limit|| p * n <= n + p)) {
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
          sharpsat_td::ShiftDel(clause, j);

          if (negative) {
            neg.push_back(clause);
          } else {
            pos.push_back(clause);
          }
          sharpsat_td::SwapDel(ins->clauses, i);
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
        sharpsat_td::SwapDel(ins->learnts, i);
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
        map[v] = var_Determined;

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
    std::unordered_map<Glucose::Var, int> table;

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
      } else if (map[var(l)] == var_Determined) {
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

    sharpsat_td::SortAndDedup(c);

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
      sharpsat_td::SwapDel(cls, i);
      i--;
    }
  }
}

template <class T_data> void Preprocessor<T_data>::Subsume() {
  {
    sharpsat_td::Subsumer subsumer;
    ins->clauses = subsumer.Subsume(ins->clauses);
    sharpsat_td::SortAndDedup(ins->clauses);
  }

  if (ins->learnts.empty())
    return;
  for (const auto &clause : ins->clauses) {
    ins->learnts.push_back(clause);
  }

  {
    sharpsat_td::Subsumer subsumer_lrnt;
    ins->learnts = subsumer_lrnt.Subsume(ins->learnts);
    sharpsat_td::SortAndDedup(ins->learnts);
  }

  for (int i = 0; i < ins->learnts.size(); i++) {
    if (std::binary_search(ins->clauses.begin(), ins->clauses.end(),
                           ins->learnts[i])) {
      sharpsat_td::SwapDel(ins->learnts, i);
      i--;
    }
  }
}

template class PRE::Preprocessor<double>;
template class PRE::Instance<double>;
