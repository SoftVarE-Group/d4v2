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

#include <algorithm>

#include "ScoringMethodHG.hpp"
#include "PartitioningHeuristicStaticSingleDual.hpp"

namespace d4 {

/**
   Constructor.

   @param[in] o, the specification of a CNF problem.
   @param[in] a, an activity manager linked to a solver.
 */
ScoringMethodHG::ScoringMethodHG(Config &config, SpecManagerCnf &o, WrapperSolver &a, std::ostream &out)
    : vsads(ScoringMethodVsads(o, a)) {
      // Instantiate the partitioner.
      partitioner = new PartitioningHeuristicStaticSingleDual(config, a, o, out);
      partitioner->init(out);
      max_score = *std::max_element(partitioner->m_bucketNumber.begin(), partitioner->m_bucketNumber.end());

      if (config.scoring_hg_scaled) {
        auto max_dlcs = o.maxDLCS();
        factor = static_cast<double>(max_dlcs) * config.scoring_hg_factor;
      } else {
        factor = config.scoring_hg_factor;
      }
}  // constructor

/**
   @param[in] v, the variable we want the score.
 */
double ScoringMethodHG::computeScore(Var v) {
  auto vsads_score = vsads.computeScore(v);
  auto hg_score = static_cast<double>(partitioner->m_bucketNumber[v]) / max_score * factor;
  return vsads_score - hg_score;
}

}  // namespace d4
