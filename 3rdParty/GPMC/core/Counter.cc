#include <gmpxx.h>
#include "core/Counter.h"
#include "mtl/Sort.h"
#include "utils/System.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace Glucose1;
using namespace GPMC;

//=================================================================================================
// Constructor

template <typename T_data>
Counter<T_data>::Counter(Configuration& config_) :
  sat					(false)
, npmodels           (0)
, config				(config_.cntr)
, tdconfig				(config_.td)
, conflicts_pre      (0)
, decisions_pre      (0)
, propagations_pre   (0)
, conflicts_sg       (0)
, decisions_sg       (0)
, propagations_sg    (0)
, sats               (0)
, nbackjumps         (0)
, nbackjumps_sp      (0)
, reduce_dbs_pre     (0)
, simp_dbs           (0)
, simplify_time      (0.0)
, npvars             (0)
, npvars_isolated    (0)
, limlevel           (0)
, last_suc           (false)
, last_bklevel       (0)
, last_cr            (CRef_Undef)
, last_lit           (lit_Undef)
, gweight				(1)
, on_bj				(config_.cntr.backjump)
, on_simp				(config_.cntr.remove_sat_cls)
, progress				(INIT)
, verbosity_c        (1)
{
	verbosity = 0;
	showModel = false;

	switch(config_.cntr.mode) {
	case MC:
		wc = false, mc = true; break;
	case WMC:
		wc = true, mc = true; break;
	case PMC:
		wc = false, mc = false; break;
	case WPMC:
		wc = true, mc = false; break;
	}

	pp.setConfig(config_.pp);
	cmpmgr.setConfig(config_.cm);
}
//=================================================================================================
// Load instance
template <typename T_data>
void Counter<T_data>::load(std::istream& in)
{
	ins.load(in, wc, !mc, config.keepVarMap);

	if(config.vs_infile != "NULL") {
		std::ifstream vin(config.vs_infile);
		if (vin) {
			ins.importVarScore(vin);
			vin.close();
		}
		else {
			std::cerr << "Cannot open file:" << config.vs_infile << std::endl;
			exit(1);
		}
	}

	progress = LOADED;
}
//=================================================================================================
// Preprocessing
template <typename T_data>
bool Counter<T_data>::preprocess()
{
	sat = pp.Simplify(&ins);

	bool done = ins.unsat || ins.vars == 0;

	if(ins.unsat) {
		npmodels = 0;
		if(config.ddnnf) cmpmgr.setRoot(BOTTOM_NODE);
		progress = COMPLETED_BYPP;
	}
	else if (ins.vars == 0) {
		if(wc) {
			npmodels = ins.gweight;
			gweight = ins.gweight;
		}
		else {
			npmodels = ((T_data)1) << ins.freevars;
			npvars_isolated = ins.freevars;
		}

		if(config.ddnnf)
			cmpmgr.setRoot(TOP_NODE);

		progress = COMPLETED_BYPP;
	}
	else {
		progress = PREPROCESSED;
		if (config.pp_outfile != "NULL") {
			printf("c o outputing simplified CNF...");
			ofstream out(config.pp_outfile);
			if (out) {
				ins.toDimacs(out);
				out.close();
				printf("done\n");
			}
			else {
				std::cerr << "Cannot open file:" << config.pp_outfile << std::endl;
			}
		}
	}

	return done;
}

// Inprocessing
template <typename T_data>
bool Counter<T_data>::simplify()
{
	if (!ok || propagate() != CRef_Undef)
		return ok = false;

	if (nAssigns() == simpDB_assigns || (simpDB_props > 0))
		return true;

	double cpu_time = cpuTime();

	// Remove satisfied clauses:
	removeSatisfied(learnts);
	if (remove_satisfied)        // Can be turned off.
		removeSatisfied(clauses);
	// checkGarbage();
	// rebuildOrderHeap();

	simpDB_assigns = nAssigns();
	simpDB_props   = clauses_literals + learnts_literals;   // (shouldn't depend on stats really, but it will do for now)

	simp_dbs++;
	simplify_time += cpuTime() - cpu_time;

	if(simp_dbs >= config.rmvsatcl_threshold)
		on_simp = false;
	return true;
}

//=================================================================================================
// Major methods:
template <typename T_data>
bool Counter<T_data>::countModels()
{
	if (progress != PREPROCESSED) {
		printf("c o The instance is not preprocessed, or model counting is already done.\n");
		return false;
	}

	import();
	solves = starts = 0;
	count_main();
	cancelUntil(0);

	progress = asynch_interrupt ? FAILED : COMPLETED;
	return !asynch_interrupt;
}

template <typename T_data>
void Counter<T_data>::count_main()
{
	int          backtrack_level;
	vec<Lit>     learnt_clause,selectors;
	unsigned int nblevels,szWoutSelectors;

	btStateT bstate = RESOLVED;
	limlevel = 0;
	cmpmgr.init(nVars(),nPVars(),clauses,ca);

	for(;;) {
		assert(!cmpmgr.topDecision().hasUnprocessedSplitComp());
		if(asynch_interrupt) return;

		int bpos = trail.size();
		for(int i = 0; i < unitcls.size(); i++){
			enqueue(unitcls[i]);
			vardata[var(unitcls[i])].level = 0;
			activity[var(unitcls[i])] = 0;
		}
		if(decisionLevel() == 0) unitcls.clear();

		CRef confl = propagate();
		if(config.watchCand) {
			for(int i = bpos; i < trail.size(); i++){
				if(var(trail[i]) < npvars && cmpmgr.isDecCand(var(trail[i]))) {
					if(wc) cmpmgr.topDecision().mulBranchWeight(lit_weight[toInt(trail[i])]);
					if(config.ddnnf) cmpmgr.addLitNode(trail[i]);
				}
			}
		}

		if(confl != CRef_Undef){
			// CONFLICT
			if(config.ddnnf) cmpmgr.addNode(BOTTOM_NODE);

			conflicts++;
			if(conflicts % 5000 == 0 && var_decay < 0.95)
				var_decay += 0.01;

			if(decisionLevel() == 0) break;

			learnt_clause.clear();
			selectors.clear();
			bool suc = analyzeMC(confl, learnt_clause, selectors, backtrack_level, nblevels, szWoutSelectors);
			if(!suc) {
				nbackjumps_sp++;
				bjResolve(backtrack_level);
				bstate = RESOLVED;
				continue;
			}

			CRef cr = CRef_Undef;
			if (learnt_clause.size() == 1){
				unitcls.push(learnt_clause[0]); nbUn++;
			}
			else {
				cr = ca.alloc(learnt_clause, true);
				learnts.push(cr);
				ca[cr].setLBD(nblevels); 
				ca[cr].setSizeWithoutSelectors(szWoutSelectors);
				if(nblevels<=2) nbDL2++;
				if(ca[cr].size()==2) nbBin++;

				attachClause(cr);
				claBumpActivity(ca[cr]);
			}

			varDecayActivity();
			claDecayActivity();

			bstate = Resolve(backtrack_level, cr, learnt_clause[0]);
		} else {
			// NO CONFLICT
			assert(!cmpmgr.topDecision().hasUnprocessedSplitComp());

			// SPLIT THE CURRENT COMPONENT
			int nsplitcomps = cmpmgr.splitComponent(assigns, lit_weight);

			if(nsplitcomps == 0) {
				// NO NEW COMPONENT (#MODELS of the current component is found)
				cmpmgr.topDecision().increaseModels((T_data)1, true);
				bstate = backtrack();
			}
			else {
				bstate = GO_TO_NEXT_COMP;

				// PROCESS COMPONENTS WITHOUT PROJECTION VARS FIRST
				lbool sat_status = l_Undef;
				while(cmpmgr.topDecision().hasUnprocessedSplitComp()
						&& !cmpmgr.topComponent().hasPVar()) {
					sat_status = solveSAT(); // SAT solving, i.e., try to find only one model

					if(sat_status == l_True) {
						sats++;
						cmpmgr.topDecision().increaseModels((T_data)1, true);
						if(!cmpmgr.topComponent().hasPVar())
							cmpmgr.cacheModelCountOf(cmpmgr.topComponent().id(),1,TOP_NODE);
						cmpmgr.eraseComponentStackID();
						cmpmgr.popComponent();
					}
					else if(sat_status == l_False) break;
					else { assert(asynch_interrupt); asynch_interrupt = true; return;}
				}

				if(sat_status == l_False){
					while(cmpmgr.topDecision().hasUnprocessedSplitComp())
						cmpmgr.popComponent();
					cmpmgr.removeCachePollutions();

					if(!last_suc) {
						nbackjumps_sp++;
						bjResolve(last_bklevel);
						bstate = RESOLVED;
						continue;
					}

					bstate = Resolve(last_bklevel, last_cr, last_lit);
				} else if(!cmpmgr.topDecision().hasUnprocessedSplitComp()){
					bstate = backtrack();
				}
			}
		}

		if(bstate == EXIT) break;
		else if(bstate == RESOLVED) continue;

		assert(cmpmgr.topComponent().hasPVar());

		if (on_simp && decisionLevel() <= limlevel && cmpmgr.checkfixedDL() && !simplify()) {
			cmpmgr.topDecision().increaseModels((T_data)0, false);
			if(config.ddnnf) cmpmgr.addNode(BOTTOM_NODE);
			bstate = backtrack();
			if(bstate == EXIT) break;
			else { assert(false); abort(); }
		}

		if(conflicts>=(uint64_t)curRestart* nbclausesbeforereduce)
		{
			assert(learnts.size()>0);
			curRestart = (conflicts/ nbclausesbeforereduce)+1;
			reduceDB();
			nbclausesbeforereduce += incReduceDB;
		}

		// DECIDE A LITERAL FROM TOP COMPONENT
		Var dec_var = cmpmgr.pickBranchVar(activity, exscore);
		decisions++;

		newDecisionLevel();
		cmpmgr.pushDecision(trail.size());

		Lit dlit = mkLit(dec_var, polarity[dec_var]);
		uncheckedEnqueue(dlit);
		if(config.watchCand) {
			if(wc) cmpmgr.topDecision().setBranchWeight(lit_weight[toInt(dlit)]);
			if(config.ddnnf) cmpmgr.setDecisionNode(dlit);
			cmpmgr.setDecCand();
		}
	}
	sat = cmpmgr.topDecision().hasModel();
	if(config.ddnnf) {
		cmpmgr.setRoot(cmpmgr.makeBranchNode(UNDEF_VAL)); // abuse makeBranchNode ...
		cmpmgr.NodeManager().compressANDs();
	}

	if(wc)
		npmodels = cmpmgr.topDecision().totalModels() * gweight;
	else
		npmodels = cmpmgr.topDecision().totalModels() << nIsoPVars();
}

template <typename T_data>
bool Counter<T_data>::analyzeMC(CRef confl, vec<Lit>& out_learnt, vec<Lit>&selectors,
		int& out_btlevel, unsigned int &lbd, unsigned int &szWithoutSelectors) {
	int pathC = 0;
	Lit p = lit_Undef;

	// Generate conflict clause:
	//
	out_learnt.push();      // (leave room for the asserting literal)
	int index = trail.size() - 1;

	do {
		assert(confl != CRef_Undef); // (otherwise should be UIP)
		Clause& c = ca[confl];

		// Special case for binary clauses
		// The first one has to be SAT
		if (p != lit_Undef && c.size() == 2 && value(c[0]) == l_False) {

			assert(value(c[1])==l_True);
			Lit tmp = c[0];
			c[0] = c[1], c[1] = tmp;
		}

		if (c.learnt())
			claBumpActivity(c);

#ifdef DYNAMICNBLEVEL
		// DYNAMIC NBLEVEL trick (see competition'09 companion paper)
		if (c.learnt() && c.lbd() > 2) {
			unsigned int nblevels = computeLBDMC(c);
			if (nblevels + 1 < c.lbd()) { // improve the LBD
				if (c.lbd() <= lbLBDFrozenClause) {
					c.setCanBeDel(false);
				}
				// seems to be interesting : keep it for the next round
				c.setLBD(nblevels); // Update it
			}
		}
#endif

		for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++) {
			Lit q = c[j];

			if (!seen[var(q)] && level(var(q)) > 0) {
				if (!isSelector(var(q)))
					varBumpActivity(var(q));
				seen[var(q)] = 1;
				if (level(var(q)) >= decisionLevel()) {
					pathC++;
#ifdef UPDATEVARACTIVITY
					// UPDATEVARACTIVITY trick (see competition'09 companion paper)
					if (!isSelector(var(q)) && (reason(var(q)) != CRef_Undef)
							&& ca[reason(var(q))].learnt())
						lastDecisionLevel.push(q);
#endif

				} else {
					if (isSelector(var(q))) {
						assert(value(q) == l_False);
						selectors.push(q);
					} else
						out_learnt.push(q);
				}
			}
		}

		// added by k-hasimt
		if (pathC <= 0) {
			// backjump conservatively to restart
			out_btlevel = 0;
			for (int i = 1; i < out_learnt.size(); i++) {
				seen[var(out_learnt[i])] = 0;
				if (out_btlevel < level(var(out_learnt[i])))
					out_btlevel = level(var(out_learnt[i]));
			}
			if (out_btlevel > 0)
				out_btlevel--;
			for (int j = 0; j < selectors.size(); j++)
				seen[var(selectors[j])] = 0;
			// ToDo: make an appropriate learnt clause and do backjumping.
			return false;
		}
		//---
		/*
		 // Select next clause to look at:
		 while (!seen[var(trail[index--])]);
		 p     = trail[index+1];
		 confl = reason(var(p));
		 seen[var(p)] = 0;
		 pathC--;
		 */
		do {
			// Select next clause to look at:
			while (!seen[var(trail[index--])])
				;
			p = trail[index + 1];
			confl = reason(var(p));
			seen[var(p)] = 0;
			pathC--;
			if (confl == CRef_Undef && pathC > 0) {
				out_learnt.push(~p);
				assert(level(var(p)) <= decisionLevel());
			}
		} while (confl == CRef_Undef && pathC > 0);

	} while (pathC > 0);
	out_learnt[0] = ~p;

	// Simplify conflict clause:
	//
	int i, j;

	for (int i = 0; i < selectors.size(); i++)
		out_learnt.push(selectors[i]);

	out_learnt.copyTo(analyze_toclear);
	if (ccmin_mode == 2) {
		uint32_t abstract_level = 0;
		for (i = 1; i < out_learnt.size(); i++)
			abstract_level |= abstractLevel(var(out_learnt[i])); // (maintain an abstraction of levels involved in conflict)

		for (i = j = 1; i < out_learnt.size(); i++)
			if (reason(var(out_learnt[i])) == CRef_Undef
					|| !litRedundant(out_learnt[i], abstract_level))
				out_learnt[j++] = out_learnt[i];

	} else if (ccmin_mode == 1) {
		for (i = j = 1; i < out_learnt.size(); i++) {
			Var x = var(out_learnt[i]);

			if (reason(x) == CRef_Undef)
				out_learnt[j++] = out_learnt[i];
			else {
				Clause& c = ca[reason(var(out_learnt[i]))];
				// Thanks to Siert Wieringa for this bug fix!
				for (int k = ((c.size() == 2) ? 0 : 1); k < c.size(); k++)
					if (!seen[var(c[k])] && level(var(c[k])) > 0) {
						out_learnt[j++] = out_learnt[i];
						break;
					}
			}
		}
	} else
		i = j = out_learnt.size();

	max_literals += out_learnt.size();
	out_learnt.shrink(i - j);
	tot_literals += out_learnt.size();

	/* ***************************************
	 Minimisation with binary clauses of the asserting clause
	 First of all : we look for small clauses
	 Then, we reduce clauses with small LBD.
	 Otherwise, this can be useless
	 */
	if (!incremental && out_learnt.size() <= lbSizeMinimizingClause) {
		minimisationWithBinaryResolution(out_learnt);
	}
	// Find correct backtrack level:
	//
	if (out_learnt.size() == 1)
		out_btlevel = 0;
	else {
		int max_i = 1;
		// Find the first literal assigned at the next-highest level:
		for (int i = 2; i < out_learnt.size(); i++)
			if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
				max_i = i;
		// Swap-in this literal at index 1:
		Lit p = out_learnt[max_i];
		out_learnt[max_i] = out_learnt[1];
		out_learnt[1] = p;
		out_btlevel = level(var(p));
	}

	// Compute the size of the clause without selectors (incremental mode)
	if (incremental) {
		szWithoutSelectors = 0;
		for (int i = 0; i < out_learnt.size(); i++) {
			if (!isSelector(var((out_learnt[i]))))
				szWithoutSelectors++;
			else if (i > 0)
				break;
		}
	} else
		szWithoutSelectors = out_learnt.size();

	// Compute LBD
	lbd = computeLBDMC(out_learnt, out_learnt.size() - selectors.size());

#ifdef UPDATEVARACTIVITY
	// UPDATEVARACTIVITY trick (see competition'09 companion paper)
	if (lastDecisionLevel.size() > 0) {
		for (int i = 0; i < lastDecisionLevel.size(); i++) {
			if (ca[reason(var(lastDecisionLevel[i]))].lbd() < lbd)
				varBumpActivity(var(lastDecisionLevel[i]));
		}
		lastDecisionLevel.clear();
	}
#endif

	for (int j = 0; j < analyze_toclear.size(); j++)
		seen[var(analyze_toclear[j])] = 0;    // ('seen[]' is now cleared)
	for (int j = 0; j < selectors.size(); j++)
		seen[var(selectors[j])] = 0;

	return true;
}

template <typename T_data>
inline void Counter<T_data>::bjResolve(int level) {
	cancelUntil(level);
	cmpmgr.backjumpTo(level);
	cmpmgr.removeCachePollutions();
	cmpmgr.topDecision().increaseModels((T_data)0, false);	// reset
	if(config.ddnnf) cmpmgr.removeChildComponentNodes();
}

template <typename T_data>
inline btStateT Counter<T_data>::Resolve(int bk_level, CRef cr, Lit lit) {
	int level = std::max(bk_level, limlevel);
	bool canjump = on_bj && level+1 < decisionLevel() && (double)level/decisionLevel() < config.bj_threshold;
	if(canjump) {
		nbackjumps++;
		bjResolve(level);
		if(cr != CRef_Undef) {
			uncheckedEnqueue(lit, cr);
			if(config.watchCand) {
				if(var(lit) < npvars && cmpmgr.isDecCand(var(lit))) {
					if(wc) cmpmgr.topDecision().mulBranchWeight(lit_weight[toInt(lit)]);
					if(config.ddnnf) cmpmgr.addLitNode(lit);
				}
			}
		}
		if(nbackjumps > nPVars()) {
			on_bj = false;
		}
		return RESOLVED;
	}
	else {
		cmpmgr.topDecision().increaseModels((T_data)0, false);	// set 0 (no model found at the current branch)
		if(config.ddnnf) cmpmgr.addNode(BOTTOM_NODE);
		return backtrack();
	}
}

template <typename T_data>
btStateT Counter<T_data>::backtrack() {
	if (decisionLevel() == 0)
		return EXIT;
	for (;;) {
		assert(decisionLevel() > 0);
		assert(!cmpmgr.topDecision().hasUnprocessedSplitComp());

		if(wc) cmpmgr.topDecision().mulLitsWeights();
		if (cmpmgr.topDecision().isFirstBranch()) {
			// FirstBranch

			Lit dlit = trail[trail_lim.last()];
			cancelCurDL();
			uncheckedEnqueue(~dlit);
			// Note: we do not try to assign an extra lit by the learnt clause when conflict occurs, but it may be an option.

			limlevel = decisionLevel();
			cmpmgr.topDecision().changeBranch();
			if(wc) cmpmgr.topDecision().setBranchWeight(lit_weight[toInt(~dlit)]);
			if(config.ddnnf) cmpmgr.setDecisionNode(~dlit);
			return RESOLVED;
		} else {
			// SecondBranch
			cmpmgr.prevDecision().increaseModels(
					cmpmgr.topDecision().totalModels(), cmpmgr.topDecision().hasModel());
			NodeIndex snode = UNDEF_NODE;
			if(config.ddnnf) {
				Var x = var(trail[trail_lim.last()]);
				snode = cmpmgr.makeBranchNode(x);
				if(snode != BOTTOM_NODE)
					cmpmgr.addNode(snode, cmpmgr.prevDecision().getNodeIdx());
			}
			cmpmgr.cacheModelCountOf(cmpmgr.topComponent().id(), cmpmgr.topDecision().totalModels(), snode);
			cmpmgr.eraseComponentStackID();

			cancelCurDL();
			trail_lim.pop();
			cmpmgr.popDecision();
			cmpmgr.popComponent();

			if (cmpmgr.topDecision().isUnSAT()) {
				while (cmpmgr.topDecision().hasUnprocessedSplitComp())
					cmpmgr.popComponent();
				cmpmgr.removeCachePollutions();
				if(config.ddnnf) cmpmgr.addNode(BOTTOM_NODE);
			} else if (cmpmgr.topDecision().hasUnprocessedSplitComp()) {
				limlevel = decisionLevel() + 1;
				return GO_TO_NEXT_COMP;
			}

			if (decisionLevel() == 0)
				return EXIT;
		}
	}

	assert(false);
	return EXIT;
}

template <typename T_data>
void Counter<T_data>::cancelCurDL() {
	for (int c = trail.size() - 1; c >= trail_lim.last(); c--) {
		Var x = var(trail[c]);
		assigns[x] = l_Undef;
		polarity[x] = sign(trail[c]);
	}
	qhead = trail_lim.last();
	trail.shrink(trail.size() - trail_lim.last());
}

template <typename T_data>
lbool Counter<T_data>::solveSAT(void) {
	newDecisionLevel();
	int sat_start_dl = decisionLevel();

	vec<Var> vs;
	Var v;
	Component & c = cmpmgr.topComponent();
	for (auto i = 0; (v = c[i]) != var_Undef; i++) {
		decision[v] = true;
		assert(value(v) == l_Undef);
		if (value(v) == l_Undef)
			vs.push(v);
	}
	order_heap.build(vs);

	solves++;

	trailQueue.fastclear();
	lbdQueue.fastclear();
	sumLBD = 0;
	conflictsRestarts = 1;

	uint64_t old_conflicts = conflicts;
	uint64_t old_decisions = decisions;
	uint64_t old_propagations = propagations;

	lbool status = l_Undef;
	while (status == l_Undef && !asynch_interrupt) {
		status = searchBelow(sat_start_dl);
	}

	conflicts_sg += conflicts - old_conflicts;
	decisions_sg += decisions - old_decisions;
	propagations_sg += propagations - old_propagations;

	for (auto i = 0; (v = c[i]) != var_Undef; i++)
		decision[v] = false;

	cancelUntil(sat_start_dl - 1);
	return status;
}

template <typename T_data>
lbool Counter<T_data>::searchBelow(int start_dl) {
	int backtrack_level;
	int conflictC = 0;
	vec<Lit> learnt_clause, selectors;
	unsigned int nblevels, szWoutSelectors;
	bool blocked = false;
	starts++;

	for (;;) {
		CRef confl = propagate();
		if (confl != CRef_Undef) {
			// CONFLICT
			conflicts++;
			conflictC++;
			conflictsRestarts++;
			if (conflicts % 5000 == 0 && var_decay < 0.95)
				var_decay += 0.01;

			if (decisionLevel() <= start_dl) {
				// LAST CONFLICT
				assert(decisionLevel() != 0);

				learnt_clause.clear();
				selectors.clear();
				last_suc = analyzeMC(confl, learnt_clause, selectors,
						last_bklevel, nblevels, szWoutSelectors);
				if (last_suc) {
					last_lit = learnt_clause[0];
					last_cr = CRef_Undef;
					if (learnt_clause.size() == 1) {
						unitcls.push(learnt_clause[0]);
						nbUn++;
					} else {
						last_cr = ca.alloc(learnt_clause, true);
						learnts.push(last_cr);
						ca[last_cr].setLBD(nblevels);
						ca[last_cr].setSizeWithoutSelectors(szWoutSelectors);
						if (nblevels <= 2)
							nbDL2++;
						if (ca[last_cr].size() == 2)
							nbBin++;

						attachClause(last_cr);
						claBumpActivity(ca[last_cr]);
					}
					varDecayActivity();
					claDecayActivity();
				}

				return l_False;
			}

			trailQueue.push(trail.size());
			// BLOCK RESTART (CP 2012 paper)
			if (conflictsRestarts > LOWER_BOUND_FOR_BLOCKING_RESTART
					&& lbdQueue.isvalid()
					&& trail.size() > R * trailQueue.getavg()) {
				lbdQueue.fastclear();
				nbstopsrestarts++;
				if (!blocked) {
					lastblockatrestart = starts;
					nbstopsrestartssame++;
					blocked = true;
				}
			}

			learnt_clause.clear();
			selectors.clear();
			analyze(confl, learnt_clause, selectors, backtrack_level, nblevels,
					szWoutSelectors);

			lbdQueue.push(nblevels);
			sumLBD += nblevels;

			cancelUntil(
					backtrack_level <= start_dl ? start_dl : backtrack_level);

			if (learnt_clause.size() == 1) {
				uncheckedEnqueue(learnt_clause[0]);
				vardata[var(learnt_clause[0])].level = 0;
				unitcls.push(learnt_clause[0]);
				nbUn++;
			} else {
				CRef cr = ca.alloc(learnt_clause, true);
				ca[cr].setLBD(nblevels);
				ca[cr].setSizeWithoutSelectors(szWoutSelectors);
				if (nblevels <= 2)
					nbDL2++;
				if (ca[cr].size() == 2)
					nbBin++;
				learnts.push(cr);
				attachClause(cr);

				claBumpActivity(ca[cr]);
				uncheckedEnqueue(learnt_clause[0], cr);
			}
			varDecayActivity();
			claDecayActivity();

		} else {
			// Our dynamic restart, see the SAT09 competition compagnion paper
			if ((lbdQueue.isvalid()
					&& ((lbdQueue.getavg() * K) > (sumLBD / conflictsRestarts)))) {
				lbdQueue.fastclear();
				// progress_estimate = progressEstimate();
				cancelUntil(start_dl);
				return l_Undef;
			}

			// Simplify the set of problem clauses:
			//if (decisionLevel() == 0 && !simplify()) {
			//  return l_False;
			//}
			// Perform clause database reduction !
			if (conflicts >= (uint64_t) curRestart * nbclausesbeforereduce) {
				assert(learnts.size() > 0);
				curRestart = (conflicts / nbclausesbeforereduce) + 1;
				reduceDB();
				nbclausesbeforereduce += incReduceDB;
			}

			decisions++;
			Lit next = pickBranchLit();
			if (next == lit_Undef)
				return l_True;

			// Increase decision level and enqueue 'next'
			newDecisionLevel();
			uncheckedEnqueue(next);
		}
	}
	return l_Undef;
}

//=================================================================================================
// Print methods:
template <typename T_data>
void Counter<T_data>::printStats() const
{
	if(verbosity_c) {
		if(progress == COMPLETED || progress == FAILED) {
			printf("c o [Statistics]\n");
			printf("c o conflicts             = %-11"PRIu64" (count %"PRIu64", sat %"PRIu64")\n", conflicts, conflicts-conflicts_sg, conflicts_sg);
			printf("c o decisions             = %-11"PRIu64" (count %"PRIu64", sat %"PRIu64")\n", decisions, decisions-decisions_sg, decisions_sg);
			printf("c o propagations          = %-11"PRIu64" (count %"PRIu64", sat %"PRIu64")\n", propagations, propagations-propagations_sg, propagations_sg);
			printf("c o simp dbs              = %-11"PRIu64" (%.3f s)\n", simp_dbs, simplify_time);
			printf("c o reduce dbs            = %-11"PRIu64"\n", nbReduceDB);
			printf("c o learnts (uni/bin/lbd2)= %"PRIu64"/%"PRIu64"/%"PRIu64"\n", nbUn, nbBin, nbDL2);
			printf("c o last learnts          = %-11d (%"PRIu64" learnts removed, %4.2f%%)\n", learnts.size(), nbRemovedClauses, nbRemovedClauses * 100 / (double)conflicts);

			printStatsOfCM();
			printf("c o isolated_pvars        = %"PRIu64"\n", nIsoPVars());
			printf("c o SAT calls             = %-11"PRIu64" (SAT %"PRIu64", UNSAT %"PRIu64")\n", solves, sats, solves-sats);
			printf("c o SAT starts            = %"PRIu64"\n", starts);
			printf("c o backjumps             = %-11"PRIu64" (sp %"PRIu64") [init %s / final %s]\n", nbackjumps+nbackjumps_sp, nbackjumps_sp, config.backjump ? "on" : "off", on_bj ? "on" : "off");
			printf("c o\n");
		}
		if(config.ddnnf) {
			printf("c o [d-DNNF Stats]\n");
			ins.printVarMapStats();
			const DTNodeManager& nmgr = cmpmgr.NodeManagerConst();
			nmgr.printStats();
			printf("c o\n");
		}

		fflush(stdout);
	}
}

template <typename T_data>
void Counter<T_data>::writeNNF() {
	if(!config.ddnnf) {
		cout << "c o No d-DNNF is constructed." << endl;
		return;
	}

	ofstream out(config.nnf_outfile);
	if (out) {
		cout << "c o writing d-DNNF to file " << config.nnf_outfile << " ... "; fflush(stdout);
		ins.writeVarMap(out);
		cmpmgr.NodeManager().printNNF(out);
		cout << "done" << endl;;
		out.close();
	}
	else {
		cerr << "Cannot open file:" << config.nnf_outfile << endl;
	}
}

template <typename T_data>
T_data Counter<T_data>::mcDDNNF() {
	if(!config.ddnnf) {
		cout << "c o No d-DNNF is constructed." << endl;
		return 0;
	}

	DTNodeManager& nm = cmpmgr.NodeManager();
	if(wc)
		return nm.countModel(lit_weight, wc) * gweight;
	else
		return nm.countModel(lit_weight, wc) << nIsoPVars();
}

//=================================================================================================
// Methods for Importing instance and additional information
template <class T_data>
void Counter<T_data>::loadWeight()
{
	if(!wc) return;

	gweight = ins.gweight;
	lit_weight.clear();
	lit_weight.growTo(2*npvars);
	for(int i=0; i<npvars; i++) {
		lit_weight[toInt(mkLit(i))] = ins.lit_weights[toInt(mkLit(i))];
		lit_weight[toInt(~mkLit(i))] = ins.lit_weights[toInt(~mkLit(i))];
	}
}

template <typename T_data>
void Counter<T_data>::import()
{
	int nvars = ins.vars;
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

	CRef cr;
	for(auto c : ins.clauses) {
		cr = ca.alloc(c, false);
		clauses.push(cr);
		attachClause(cr);
	}
	for(auto c : ins.learnts) {
		cr = ca.alloc(c, true);
		clauses.push(cr);
		attachClause(cr);
	}

	npvars = ins.npvars;
	npvars_isolated = ins.freevars;

	sat = !ins.unsat;
	loadWeight();
}

template <typename T_data>
void Counter<T_data>::setExtraVarScore()
{
	exscore.clear();
	exscore.growTo(ins.npvars, 0);

	if(config.vs_infile != "NULL")
		setGivenVarScore();
	else if(config.doTD)
		computeTDScore();
}

template <typename T_data>
void Counter<T_data>::computeTDScore()
{
	bool conditionOnCNF = ins.vars > 20 && ins.vars <= tdconfig.varlim && ins.learnts.size() <= ins.clauses.size();
	if(!(conditionOnCNF || config.alwTD)) {
		printf("c o skip td\n");
		return;
	}

	Graph Primal(ins.vars, ins.clauses);
	printf("c o Primal graph: nodes %d, edges %d\n", ins.vars, Primal.numEdges());

	bool conditionOnPrimalGraph =
			(double)Primal.numEdges()/((long) ins.vars * ins.vars) <= tdconfig.denselim
			&& (double)Primal.numEdges()/ins.vars <= tdconfig.ratiolim;
	if(!(conditionOnPrimalGraph || config.alwTD)) {
		printf("c o skip td\n");
		return;
	}

	// run FlowCutter
	printf("c o FlowCutter is running...\n");fflush(stdout);
	IFlowCutter FC(ins.vars, Primal.numEdges(), tdconfig.timelim);
	FC.importGraph(Primal);
	Primal.clear();
	TreeDecomposition td = FC.constructTD();

	bool uselessTD = true;
	if(td.numNodes() > 0) {	// if TD construction is successful
		// find a centroid of the constructed TD
		int centroid = td.centroid(ins.npvars);

		// write the the constructed TD (if needed)
		if(config.td_outfile != "NULL") {
			ofstream out(config.td_outfile);
			if(out) {
				td.toDimacs(out, centroid+1, ins.npvars);
				out.close();
			} else {
				std::cerr << "Cannot open file:" << config.td_outfile << std::endl;
			}
		}

		bool conditionOnTreeWidth = (double)td.width()/ins.npvars < tdconfig.twvarlim;
		if(conditionOnTreeWidth || config.alwTD) {
			std::vector<int> dists = td.distanceFromCentroid(ins.npvars);
			if(!dists.empty()) {
				int max_dst = 0;
				for(int i=0; i<ins.vars; i++)
					max_dst = max(max_dst, dists[i]);
				if(max_dst > 0) {
					for(int i=0; i<ins.npvars; i++)
						exscore[i] = config.coef_tdscore * ((double)(max_dst - dists[i])) / (double)max_dst;
					uselessTD = false;
				}
			}
		}
	}

	if(uselessTD)
		printf("c o ignore td\n");
}

template <typename T_data>
void Counter<T_data>::setGivenVarScore() {
	if(!config.keepVarMap || ins.score.size() == 0) return;

	for(int i=0; i<ins.gmap.size(); i++) {
		if(ins.gmap[i] == lit_Undef) continue;

		Var x = var(ins.gmap[i]);
		assert(x < ins.npvars);
		if(exscore[x] < ins.score[i])
			exscore[x] = ins.score[i];
	}
	ins.score.clear();
	ins.score.shrink_to_fit();
}

//=================================================================================================
// Inline Methods
// NOTE: These are copies of the inline methods in Glucose1.

template <typename T_data>
inline unsigned int Counter<T_data>::computeLBDMC(const vec<Lit> & lits,int end) {
	int nblevels = 0;
	MYFLAG++;

	if(incremental) { // ----------------- INCREMENTAL MODE
		if(end==-1) end = lits.size();
		unsigned int nbDone = 0;
		for(int i=0;i<lits.size();i++) {
			if(nbDone>=end) break;
			if(isSelector(var(lits[i]))) continue;
			nbDone++;
			int l = level(var(lits[i]));
			if (permDiff[l] != MYFLAG) {
				permDiff[l] = MYFLAG;
				nblevels++;
			}
		}
	} else { // -------- DEFAULT MODE. NOT A LOT OF DIFFERENCES... BUT EASIER TO READ
		for(int i=0;i<lits.size();i++) {
			int l = level(var(lits[i]));
			if (permDiff[l] != MYFLAG) {
				permDiff[l] = MYFLAG;
				nblevels++;
			}
		}
	}

	return nblevels;
}

template <typename T_data>
inline unsigned int Counter<T_data>::computeLBDMC(const Clause &c) {
	int nblevels = 0;
	MYFLAG++;

	if(incremental) { // ----------------- INCREMENTAL MODE
		int nbDone = 0;
		for(int i=0;i<c.size();i++) {
			if(nbDone>=c.sizeWithoutSelectors()) break;
			if(isSelector(var(c[i]))) continue;
			nbDone++;
			int l = level(var(c[i]));
			if (permDiff[l] != MYFLAG) {
				permDiff[l] = MYFLAG;
				nblevels++;
			}
		}
	} else { // -------- DEFAULT MODE. NOT A LOT OF DIFFERENCES... BUT EASIER TO READ
		for(int i=0;i<c.size();i++) {
			int l = level(var(c[i]));
			if (permDiff[l] != MYFLAG) {
				permDiff[l] = MYFLAG;
				nblevels++;
			}
		}
	}
	return nblevels;
}

template class GPMC::Counter<mpz_class>;
