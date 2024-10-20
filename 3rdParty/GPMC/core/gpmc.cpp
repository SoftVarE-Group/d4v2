#include "gpmc.hpp"
#include <iostream>

#include "core/Config.h"
#include "core/Counter.h"
#include "core/Dimacs.h"
#include "core/Instance.h"
#include "preprocessor/Preprocessor.h"
#include "utils/Options.h"
#include "utils/ParseUtils.h"
#include "utils/System.h"

namespace GPMC {
using namespace PPMC;

bool simplify(std::vector<std::vector<d4::Lit>> &clauses,
              std::vector<d4::Var>& sel,
              std::vector<std::vector<d4::Lit>> &learnts, int &vars, int &pvars,int &freevars,
              std::vector<d4::Lit> &assigns, std::vector<d4::Lit> &gmap,bool equiv,bool check) {

  Instance<mpz_class> ins;
  ins.weighted = false;
  ins.projected = vars != pvars;
  ins.keepVarMap = true;
  ins.vars = vars;
  ins.npvars = sel.size();
  ins.ispvars.resize(vars, false);
  ins.assigns.resize(vars, l_Undef);
  for (auto v:sel) {
    ins.pvars.push_back(v-1);
    ins.ispvars[v-1] = true;
  }
  for (auto &cl : clauses) {
    std::vector<Lit> lits(cl.size());
    for (int i = 0; i < cl.size(); i++) {
      lits[i] = mkLit(cl[i].var() - 1, cl[i].sign());
    }
    ins.addClause(lits, false);
  }
  if (ins.projected) {
    for (auto v : ins.pvars)
      ins.gmap.push_back(mkLit(v));
  } else {
    for (int i = 0; i < ins.npvars; i++)
      ins.gmap.push_back(mkLit(i));
  }

  Configuration config; 
  config.pp.ve_dve = !equiv;
  config.pp.ve_check = check;
  Preprocessor<mpz_class> preproc;
  preproc.setConfig(config.pp);
  preproc.Simplify(&ins);
  if (ins.unsat) {
    return false;
  }
  std::cout << "Preproc Done! Vars: " << ins.vars << " before " << vars
            << " Clauses: " << ins.clauses.size() << " before "
            << clauses.size() << " Lerants: " << ins.learnts.size()
            << " PVars: " << ins.npvars << " before " << pvars << std::endl;
  // ins.writeVarMap(std::cout);
  //ins.printVarMapStats();
 //ins.writeVarMap(std::cout);
  clauses.clear();
  for (auto cl : ins.clauses) {
    clauses.push_back({});
    auto &ncl = clauses.back();
    ncl.resize(cl.size());
    for (auto k = 0; k < cl.size(); k++) {
      ncl[k] = d4::Lit::makeLit(var(cl[k]) + 1, sign(cl[k]));
      assert(ncl[k].var()>0);
    }
  }
  for (auto cl : ins.learnts) {
    learnts.push_back({});
    auto &ncl = learnts.back();
    ncl.resize(cl.size());
    for (auto k = 0; k < cl.size(); k++) {
      ncl[k] = d4::Lit::makeLit(var(cl[k]) + 1, sign(cl[k]));
    }
  }
  vars = ins.vars;
  pvars = ins.npvars;
  freevars = ins.freevars;
  return true;
}

} // namespace GPMC
