
#include "TestSolver.hpp"
#include <iostream>

namespace PRE{
using namespace std;
using namespace Glucose;


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
    scoreActivity.growTo(nvars,0.0);
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
// Graph
Graph::Graph(int vars, const vector<vector<Glucose::Lit>>& clauses)
{
	clear();
	init(vars);
	for(const auto& clause : clauses)
		for(int i=0; i<clause.size(); i++)
			for(int j=i+1; j<clause.size(); j++)
				addEdge(var(clause[i]), var(clause[j]));
}
Graph::Graph(int vars, const vector<vector<Glucose::Lit>>& clauses, const vector<vector<Glucose::Lit>>& learnts, vector<int>& freq)
{
	clear();
	init(vars);
	freq.resize(vars*2, 0);

	for(const auto& cls : {clauses, learnts}) {
		for(const auto& clause : cls) {
			for(int i=0; i<clause.size(); i++) {
				freq[toInt(clause[i])]++;
				for(int j=i+1; j<clause.size(); j++)
					addEdge(var(clause[i]), var(clause[j]));

			}
		}
	}
}

Graph::Graph(int vars, const vector<vector<Glucose::Lit>>& clauses, const vector<vector<Glucose::Lit>>& learnts, vector<int>& freq,std::vector<double>& cl_size)
{
	clear();
	init(vars);
	freq.resize(vars*2, 0);
    cl_size.resize(vars,0);

	for(const auto& cls : {clauses, learnts}) {
		for(const auto& clause : cls) {
			for(int i=0; i<clause.size(); i++) {
				freq[toInt(clause[i])]++;
				cl_size[var(clause[i])]+=clause.size();
				for(int j=i+1; j<clause.size(); j++)
					addEdge(var(clause[i]), var(clause[j]));

			}
		}
	}
    for(int i = 0;i<cl_size.size();i++){
        int sum = freq[toInt(Glucose::mkLit(i))]+freq[toInt(~Glucose::mkLit(i))];
        if(sum>0){
            cl_size[i] /=sum;
        }

    }

}
void Graph::init(int n)
{
	nodes = n;

	adj_list.clear();
	adj_list.resize(nodes, {});

	adj_mat.clear();
	adj_mat.resize(n);
	for(int i=0; i<n; i++) {
		adj_mat[i] = sharpsat_td::Bitset(n);
		adj_mat[i].SetFalse(i);
	}
}
void Graph::clear()
{
	nodes = 0;
	edges = 0;
	adj_list.clear();
	adj_mat.clear();
}
void Graph::addEdge(int v1, int v2)
{
	if(adj_mat[v1].Get(v2))
		return;

	adj_list[v1].push_back(v2);
	adj_list[v2].push_back(v1);
	adj_mat[v1].SetTrue(v2);
	adj_mat[v2].SetTrue(v1);
	edges++;
}
bool Graph::isClique(const vector<int>& adj)
{
	for(int i=0; i<adj.size(); i++)
		for(int j=i+1; j<adj.size(); j++)
			if(!hasEdge(adj[i], adj[j])) return false;

	return true;
}

void Graph::toDimacs(ostream& out, bool withHeader)
{
	if(withHeader)
		out << "p tw "<< nodes << " " << edges << endl;

	for(int i=0; i<nodes; i++) {
		for(auto j : adj_list[i]) {
			if(i>=j) continue;
			out << (i+1) << " " << (j+1) << endl;
		}
	}
}

}
