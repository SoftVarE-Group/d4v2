#ifndef TestSolver_h
#define TestSolver_h

#include <vector>

#include "core/Solver.h"

namespace PPMC {

class TestSolver: public Glucose1::Solver {
public:
	TestSolver(int nvars) { newVars(nvars); }
	TestSolver(int nvars,
			std::vector<std::vector<Glucose1::Lit>> clauses,
			std::vector<std::vector<Glucose1::Lit>> learnts,
			std::vector<Glucose1::Lit> assignedLits);

	void addClauseWith(const std::vector<Glucose1::Lit>& ps, bool learnt=false);
	void resetClauses(std::vector<std::vector<Glucose1::Lit>>& clauses);

	Glucose1::lbool Solve() { return solve_(); }
	Glucose1::lbool Solve(const Glucose1::vec<Glucose1::Lit>& assumptions) { budgetOff(); setConfBudget(clauses.size()*10); return solveLimited(assumptions);}

	bool falsifiedBy(Glucose1::Lit l);
	bool falsifiedBy(Glucose1::Lit l1, Glucose1::Lit l2);
	bool falsifiedBy(Glucose1::vec<Glucose1::Lit>& assump);
	bool FailedLiterals();

	void exportLearnts(std::vector<std::vector<Glucose1::Lit>>& learnts);

	void assign(Glucose1::Lit l) { enqueue(l); }
	bool bcp() { return propagate() == Glucose1::CRef_Undef; }

	Glucose1::vec<Glucose1::lbool>& getAssigns() { return assigns; }
	Glucose1::vec<Glucose1::Lit>& getTrail() { return trail; }

private:
	void newVars(int nvars);
	void backTo(int pos);

};
}

#endif
