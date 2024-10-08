
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

#pragma once

#include <iostream>
#include <vector>

#include "HyperGraph.hpp"
#include "HyperGraphExtractor.hpp"
#include "HyperGraphExtractorDual.hpp"
#include "src/problem/ProblemTypes.hpp"
#include "src/specs/cnf/SpecManagerCnf.hpp"
namespace d4 {
class HyperGraphExtractorDualProj : public HyperGraphExtractorDual {
private:
  void compute_stats(SpecManagerCnf& specs);
  std::vector<int> m_cost;
  SpecManagerCnf& m_specs;
  int m_nproj_cost;

public:
  HyperGraphExtractorDualProj(unsigned nbVar, unsigned nbClause, SpecManagerCnf& specs,int nproj_cost=100);

  void constructHyperGraph(SpecManagerCnf &om, std::vector<Var> &component,
                           std::vector<Var> &equivClass,
                           std::vector<std::vector<Var>> &equivVar,
                           bool reduceFormula, std::vector<Var> &considered,
                           HyperGraph &hypergraph);


};
} // namespace d4
