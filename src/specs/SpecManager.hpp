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
#include "src/utils/Proj.hpp"
#include <boost/program_options.hpp>
#include <ranges>
#include <src/problem/ProblemManager.hpp>
#include <src/problem/ProblemTypes.hpp>
#include <vector>

namespace d4 {
namespace po = boost::program_options;
class SpecManager {
protected:
  unsigned m_nbVar;
  unsigned m_nbProj;

public:
  static SpecManager *makeSpecManager(Config &config, ProblemManager &p,
                                      std::ostream &out);

  inline bool isSelected(Var v) { return v <= m_nbProj; }
  inline bool isProj() { return m_nbProj != m_nbVar; }
  inline int nbSelected() { return m_nbProj; }

  virtual ~SpecManager() {}
  virtual bool litIsAssigned(Lit l) = 0;
  virtual bool litIsAssignedToTrue(Lit l) = 0;
  virtual bool varIsAssigned(Var v) = 0;
  virtual int
  computeConnectedComponent(std::vector<std::vector<Var>> &varConnected,
                            std::vector<Var> &setOfVar,
                            std::vector<Var> &freeVar) = 0;
  virtual int computeConnectedComponent(std::vector<ProjVars> &varConnected,
                                        std::vector<Var> &setOfVar,
                                        std::vector<Var> &freeVar) = 0;
  virtual void preUpdate(std::vector<Lit> &lits) = 0;
  virtual void preUpdate(std::vector<Lit> &lits,std::vector<Lit>& pure ) = 0;
  virtual void postUpdate(std::vector<Lit> &lits) = 0;
  virtual void initialize(std::vector<Var> &setOfVar,
                          std::vector<Lit> &units) = 0;
  virtual void showFormula(std::ostream &out) = 0;
  virtual void showCurrentFormula(std::ostream &out) = 0;
  virtual void showTrail(std::ostream &out) = 0;
  virtual int getNbOccurrence(Lit l) = 0;
  virtual int getNbVariable() = 0;
};
} // namespace d4
