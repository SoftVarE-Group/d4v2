/*
 * d4
 * Copyright (C) 2020  Univ. Artois & CNRS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "ScoringMethod.hpp"

#include "cnf/ScoringMethodDlcs.hpp"
#include "cnf/ScoringMethodJwts.hpp"
#include "cnf/ScoringMethodMom.hpp"
#include "cnf/ScoringMethodVsads.hpp"
#include "cnf/ScoringMethodVsids.hpp"
#include "src/exceptions/FactoryException.hpp"
#include "src/heuristics/cnf/ScoringMethodHG.hpp"

namespace d4 {

/**
   Select from the arguments store in vm the good scoring method and return it.

   @param[in] vm, the arguments on the command line.
   @pararm[in] p, the problem manager.

   \return the scoring method
 */
ScoringMethod *ScoringMethod::makeScoringMethod(Config &config,
                                                SpecManager &p,
                                                WrapperSolver &am,
                                                std::ostream &out) {
  std::string inType = config.input_type;
  std::string meth = config.scoring_method;
  out << "c [CONSTRUCTOR] Variable heuristic: " << meth << "\n";

  if (inType == "cnf" || inType == "dimacs") {
    try {
      auto &ps = dynamic_cast<SpecManagerCnf &>(p);
      if (meth == "mom")
        return new ScoringMethodMom(ps);
      if (meth == "dlcs")
        return new ScoringMethodDlcs(ps);
      if (meth == "vsids")
        return new ScoringMethodVsids(am);
      if (meth == "vsads")
        return new ScoringMethodVsads(ps, am);
      if (meth == "hg")
        return new ScoringMethodHG(config, ps, am, out);
      if (meth == "jwts")
        return new ScoringMethodJwts(ps);
      return nullptr;
    } catch (std::bad_cast &bc) {
      std::cerr << "bad_cast caught: " << bc.what() << '\n';
      std::cerr << "A CNF formula was expeted\n";
    }
  }

  throw(FactoryException("Cannot create a ScoringMethod", __FILE__, __LINE__));
} // makeScoringMethod

/**
   Select the best variable in vars and return it.

   \param[in] vars, the set of variables we search in.

   \return the best variable if exists, var_Undef otherwise.
 */
Var ScoringMethod::selectVariable(std::vector<Var> &vars, SpecManager &s,
                                  std::vector<bool> &isDecisionVariable) {
  Var ret = var_Undef;
  double bestScore = -1;
  assert(isDecisionVariable.size() >= (unsigned)s.getNbVariable());

  for (auto &v : vars) {
    if (s.varIsAssigned(v) || !isDecisionVariable[v])
      continue;
    if (ret != var_Undef && s.varIsAssigned(v)) {
      break;
    }

    double current = computeScore(v);
    if (ret == var_Undef || current > bestScore) {
      ret = v;
      bestScore = current;
    }
  }
  //sleep(1);

  return ret;
} // selectVariable
  //
Var ScoringMethod::selectVariable(std::vector<Var> &vars, SpecManager &s) {
  Var ret = var_Undef;
  double bestScore = -1;

  for (auto &v : vars) {
    if (s.varIsAssigned(v) || !s.isSelected(v))
      continue;

    double current = computeScore(v);
    if (ret == var_Undef || current > bestScore) {
      ret = v;
      bestScore = current;
    }
  }
  return ret;
} // selectVariable
Var ScoringMethod:: selectVariable(std::vector<Var> &vars,std::function<bool(Var)> can_select){
  Var ret = var_Undef;
  double bestScore = -1;

  for (auto &v : vars) {
    if(!can_select(v))
      continue;
    double current = computeScore(v);
    if (ret == var_Undef || current > bestScore) {
      ret = v;
      bestScore = current;
    }
  }
  return ret;

}

} // namespace d4
