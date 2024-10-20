#include "TestSolver.h"
#include <iostream>

using namespace PPMC;
using namespace Glucose1;
using namespace std;

TestSolver::TestSolver(int nvars,
		vector<vector<Lit>> clauses_,
		vector<vector<Lit>> learnts_,
		vector<Lit> assignedLits)
{    
	newVars(nvars);
	for(auto l : assignedLits) {
		uncheckedEnqueue(l);
	}
	for(auto c : clauses_)
		addClauseWith(c);
	for(auto c : learnts_)
		addClauseWith(c, true);

	if(!assignedLits.empty()) {
		CRef confl = propagate();
		ok = (propagate() == CRef_Undef);
	}
}

void TestSolver::newVars(int nvars)
{
    watches  .init(mkLit(nvars-1, true ));
    watchesBin  .init(mkLit(nvars-1, true ));
    assigns  .growTo(nvars, l_Undef);
    vardata  .growTo(nvars, mkVarData(CRef_Undef, 0));
    activity .growTo(nvars, 0);
    seen     .growTo(nvars, 0);
    permDiff  .growTo(nvars, 0);
    polarity .growTo(nvars, true);
    decision .growTo(nvars);
    trail    .capacity(nvars);
    for(int i=0; i<nvars; i++)
    	setDecisionVar(i, true);
}

void TestSolver::addClauseWith(const vector<Lit>& ps, bool learnt)
{
	CRef cr = ca.alloc(ps, learnt);
	if(!learnt)
		clauses.push(cr);
	else
		learnts.push(cr);
	attachClause(cr);
}

inline void TestSolver::backTo(int pos)
{
	for(int c = trail.size()-1; c >= pos; c--){
		Var x = var(trail[c]);
		assigns[x] = l_Undef;
	}
	qhead = pos;
	trail.shrink(trail.size() - pos);
}

bool TestSolver::falsifiedBy(Lit assump)
{
	int sz = trail.size();
	uncheckedEnqueue(assump);
	CRef confl = propagate();
	backTo(sz);

	return confl != CRef_Undef;
}

bool TestSolver::falsifiedBy(Lit l1, Lit l2)
{	
	vec<Lit> ps;

	ps.push(l1);
	ps.push(l2);
	return falsifiedBy(ps);
}

bool TestSolver::falsifiedBy(vec<Lit>& assump)
{
	int         backtrack_level;
	vec<Lit>    learnt_clause, selectors;
	unsigned int nblevels,szWoutSelectors;
	lbool result = l_Undef;

	for (;;){
		CRef confl = propagate();
		if (confl != CRef_Undef){
			// CONFLICT
			if (decisionLevel() == 0) {
				result = l_False;
				break;
			}

			learnt_clause.clear();
			selectors.clear();
			analyze(confl, learnt_clause, selectors, backtrack_level, nblevels, szWoutSelectors);

			cancelUntil(backtrack_level);

			if (learnt_clause.size() == 1) {
				assert(value(learnt_clause[0])==l_Undef);
				uncheckedEnqueue(learnt_clause[0]);
			}else{
				CRef cr = ca.alloc(learnt_clause, true);
				ca[cr].setLBD(nblevels);
				ca[cr].setSizeWithoutSelectors(szWoutSelectors);
				learnts.push(cr);
				attachClause(cr);

				claBumpActivity(ca[cr]);
				uncheckedEnqueue(learnt_clause[0], cr);
			}
			claDecayActivity();

		}else{
			Lit next = lit_Undef;

			assert(decisionLevel() <= assump.size());

			while (decisionLevel() < assump.size()) {
				Lit p = assump[decisionLevel()];
				if (value(p) == l_True){
					newDecisionLevel();
				}else if (value(p) == l_False){
					learnt_clause.clear();
					analyzeFinal(~p, learnt_clause);

					if (learnt_clause.size() == 1){
						cancelUntil(0);
						enqueue(learnt_clause[0]);
						propagate();
					}else{
						CRef cr = ca.alloc(learnt_clause, true);
						ca[cr].setLBD(nblevels);
						ca[cr].setSizeWithoutSelectors(szWoutSelectors);
						learnts.push(cr);
						attachClause(cr);
						claBumpActivity(ca[cr]);
					}
					claDecayActivity();
					result = l_False;
					goto End;
				}else{
					next = p;
					break;
				}
			}

			if (decisionLevel() == assump.size()) break;

			newDecisionLevel();
			uncheckedEnqueue(next);
		}
	}

End:
	cancelUntil(0);
	return result == l_False;
}


bool TestSolver::FailedLiterals()
{
	assert(decisionLevel()==0);
	int minv = min(nVars(), 1000000);

	int last_size;
	do {
		last_size = trail.size();

		for (Var v = 0; v < minv; v++)
			if (value(v) == l_Undef) {
				if(falsifiedBy(mkLit(v,true))) {
					uncheckedEnqueue(mkLit(v, false));
					if(propagate() != CRef_Undef) return false;
				} else if(falsifiedBy(mkLit(v,false))) {
					uncheckedEnqueue(mkLit(v, true));
					if(propagate() != CRef_Undef) return false;
				}
			}

	} while (trail.size() > last_size);

	return true;
}

void TestSolver::exportLearnts(vector<vector<Lit>>& ilearnts)
{
	ilearnts.clear();

	if(learnts.size() == 0) return;

	if (learnts.size() >= clauses.size())
		reduceDB();

	for(int i=0; i<learnts.size(); i++){
		Clause& c = ca[learnts[i]];
		ilearnts.push_back({});
		for(int j=0; j<c.size(); j++) {
			if(value(c[j]) == l_True) {
				ilearnts.pop_back();
				break;
			} else if(value(c[j]) == l_Undef) {
				ilearnts.back().push_back(c[j]);
			}
		}
	}
}

void TestSolver::resetClauses(vector<vector<Lit>>& cls)
{
	for(int i=0; i<clauses.size(); i++) {
		CRef cr = clauses[i];
		removeClause(cr);
	}
	clauses.clear();


	for(auto c : cls) {
		assert(c.size() > 0);
		vec<Lit> tmp;
		for(auto l : c)
			tmp.push(l);
		addClause_(tmp);
	}
}

