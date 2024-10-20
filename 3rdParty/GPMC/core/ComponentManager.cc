#include "core/Component.h"
#include "mtl/Sort.h"
#include <utility>

using namespace GPMC;

#define CACHE

template <typename T_data>
void ComponentManager<T_data>::init(int nvars, int npvars, const vec<CRef>& sclauses, const ClauseAllocator& sca){
	vec< vec<Var> > binlinks(nvars);
	vec< vec<ClID> > occ(nvars);

	occlists_.growTo(nvars);
	var_frequency_.growTo(nvars);

	for(int i = 0; i < sclauses.size(); i++) {
		Clause& sc = (Clause &)sca[sclauses[i]];
		if(sc.size() == 2) {
			binlinks[var(sc[0])].push(var(sc[1]));
			binlinks[var(sc[1])].push(var(sc[0]));
			var_frequency_[var(sc[0])]++;
			var_frequency_[var(sc[1])]++;
		}
		else {	// sc.size() > 2 (because bcp was already done and satisfied clauses were removed)
			clauses_.push(ca_.alloc(sc));
			for(int j = 0; j < sc.size(); j++){
				occ[var(sc[j])].push(clauses_.size()-1);
				var_frequency_[var(sc[j])]++;
			}
		}
	}

	occ_pool_.clear();
	for(Var v = 0; v < nvars; v++) {
		occlists_[v] = occ_pool_.size();

		vec<Var>& bl = binlinks[v];
		sort(bl);
		Var prev = var_Undef;
		for(int i = 0; i < bl.size(); i++)
			if(bl[i] != prev) {
				occ_pool_.push(bl[i]);
				prev = bl[i];
			}
		occ_pool_.push(var_Undef);

		vec<ClID>& ocl = occ[v];
		for(int i = 0; i < ocl.size(); i++)
			occ_pool_.push(ocl[i]);
		occ_pool_.push(clid_Undef);
	}

	varseen_.growTo(nvars, SC_CANDIDATE);
	clseen_.growTo(clauses_.size(), SC_CANDIDATE);
	npvars_ = npvars;

#ifdef CACHE
	cache_.init(config.cachesize);
	CachedComponent<T_data>::adjustPackSize(nvars, clauses_.size());
#endif

	nodeMgr.init(npvars_);

	initComponentStack(nvars, clauses_.size());
	initDecisionStack();

	components = 1;
	num_try_split = 0;
}

template <typename T_data>
void ComponentManager<T_data>::initComponentStack(int nvars, int nlongcls) {
	comp_stack_.clear();
	comp_stack_.reserve(nvars + 1);
	comp_stack_.push_back(new Component(nvars, nlongcls, true));

	Component & orig = *(comp_stack_[0]);
	for(Var  v = 0; v < nvars; v++)    orig.pushVar(v);
	orig.closeVar();
	for(ClID c = 0; c < nlongcls; c++) orig.pushClID(c);
	orig.closeClID();

#ifdef CACHE
	CacheEntryID id = cache_.createEntryFor(*comp_stack_.back(), comp_stack_.size() - 1);
	comp_stack_.back()->set_id(id);
#endif
}

template <typename T_data>
void ComponentManager<T_data>::initDecisionStack()
{
	dl_.clear();
	dl_.push_back(Decision<T_data>(0,0));
	dl_.back().changeBranch();
	dl_.back().setNodeIdx(nodeMgr.newNode(DT_AND));
}

template <typename T_data>
int ComponentManager<T_data>::splitComponent(const vec<lbool>& assigns, const vec<T_data>& lit_weight){
	int p; Var v; ClID c;
	T_data tmp_model_count;
	NodeIndex cachedNode = UNDEF_NODE;

	unsigned oldtop = comp_stack_.size();
	unsigned boundary = oldtop;
	Component & targetcomp =  *(comp_stack_.back());

	for(p = 0; (v = targetcomp[p]) != var_Undef; p++)
		if(value(v, assigns) == l_Undef){
			varseen_[v] = SC_CANDIDATE;
			var_frequency_[v] = 0;
		}
		else varseen_[v] = SC_NOT_CANDIDATE;

	for(++p; (c = targetcomp[p]) != clid_Undef; p++)
		clseen_[c] = SC_CANDIDATE;


	// Start splitting
	int nvars_in_comp, ncls_in_comp;
	for (p = 0; (v = targetcomp[p]) != var_Undef; p++) {
		if (varseen_[v] == SC_CANDIDATE) {
			searchComponent(v, assigns, nvars_in_comp, ncls_in_comp);
			if (nvars_in_comp == 1) {
				if(isPVar(v)){
					if(config.weighted)
						dl_.back().increaseModels(lit_weight[toInt(mkLit(v, true))]+lit_weight[toInt(mkLit(v, false))], true);
					else
						dl_.back().increaseModels((T_data)2, true);

					if(config.ddnnf) addNode(nodeMgr.getDCNode(v));
				}
				else
					dl_.back().increaseModels((T_data)1, true);
				varseen_[v] = SC_DONE;
			}else{
				comp_stack_.push_back(new Component(nvars_in_comp, ncls_in_comp, isPVar(v)));

				Component & newcomp = *(comp_stack_.back());

				int pp; Var vv; ClID cc;
				for (pp = 0; (vv = targetcomp[pp]) != var_Undef; pp++)
					if (varseen_[vv] == SC_SEEN) {
						newcomp.pushVar(vv);
						varseen_[vv] = SC_DONE;
					}
				newcomp.closeVar();
				for (++pp; (cc = targetcomp[pp]) != clid_Undef; pp++)
					if (clseen_[cc] == SC_SEEN) {
						newcomp.pushClID(cc);
						clseen_[cc] = SC_DONE;
					}
				newcomp.closeClID();
#ifndef CACHE
				if(isPVar(v)) boundary++;
#endif
#ifdef CACHE
				// caching
				CacheEntryID id = cache_.createEntryFor(*comp_stack_.back(), comp_stack_.size() - 1);
				if (id != 0) {
					comp_stack_.back()->set_id(id);
					assert(cache_.hasEntry(id));
					assert(cache_.hasEntry(targetcomp.id()));
					if (cache_.requestValueOf(*comp_stack_.back(), tmp_model_count, cachedNode)) {
						dl_.back().increaseModels(tmp_model_count, true);
						if(config.ddnnf && cachedNode != TOP_NODE) {
							addNode(cachedNode);
						}
						cache_.eraseEntry(id);
						delete comp_stack_.back();
						comp_stack_.pop_back();
					} else {
						cache_.entry(id).set_father(targetcomp.id());
						cache_.add_descendant(targetcomp.id(), id);
						if(isPVar(v)) boundary++;
					}
				}
#endif
			}
		}
	}

	Decision<T_data>& curdl = dl_.back();
	curdl.setSplitCompsEnd(comp_stack_.size());

	for (auto i = oldtop; i < boundary; i++)
		for (auto j = i + 1; j < boundary; j++)
			if (comp_stack_[i]->nVars()< comp_stack_[j]->nVars())
				std::swap(comp_stack_[i], comp_stack_[j]);

	components += comp_stack_.size() - oldtop;
	if(comp_stack_.size() != oldtop) num_try_split++;
	return comp_stack_.size() - oldtop;
}

template <class T_data>
void ComponentManager<T_data>::searchComponent(Var seed_var, const vec<lbool>& assigns, int& nvars_in_comp, int& ncls_in_comp){
	vec<Var> vars_in_comp;

	ncls_in_comp = 0;
	vars_in_comp.push(seed_var);
	varseen_[seed_var] = SC_SEEN;

	int p; Var v1, v2; ClID c;
	for(p = 0; p < vars_in_comp.size(); p++){
		v1 = vars_in_comp[p];

		uint32_t q;
		for(q = occlists_[v1]; (v2 = occ_pool_[q]) != var_Undef; q++){
			if(varseen_[v2] == SC_CANDIDATE) {
				assert(value(v2,assigns)==l_Undef);
				vars_in_comp.push(v2);
				varseen_[v2] = SC_SEEN;
				var_frequency_[v1]++;
				var_frequency_[v2]++;
			}
		}

		int nlits;
		for(++q; (c = occ_pool_[q]) != clid_Undef; q++) {
			if(clseen_[c] == SC_CANDIDATE) {
				Clause& cl = ca_[clauses_[c]];
				int before = vars_in_comp.size();
				nlits = 0;
				for(int i = 0; i < cl.size(); i++) {
					if(value(cl[i],assigns) == l_False) continue;
					else if(value(cl[i],assigns) == l_True){
						while(before < vars_in_comp.size()){
							varseen_[vars_in_comp.last()] = SC_CANDIDATE;
							vars_in_comp.pop();
						}
						clseen_[c] = SC_NOT_CANDIDATE;
						for(int j = 0; j < i; j++) {
							if(var_frequency_[var(cl[j])] > 0)
								var_frequency_[var(cl[j])]--;
						}
						goto NEXT;
					}
					else {
						v2 = var(cl[i]);
						assert(value(v2,assigns)==l_Undef);
						nlits++;
						var_frequency_[v2]++;
						if(varseen_[v2] == SC_CANDIDATE){
							varseen_[v2] = SC_SEEN;
							vars_in_comp.push(v2);
						}
					}
				}
				clseen_[c] = SC_SEEN;
				ncls_in_comp++;
			}
			NEXT:;
		}
	}

	nvars_in_comp = vars_in_comp.size();

}

template <class T_data>
Var ComponentManager<T_data>::pickBranchVar(const vec<double>& activity, const vec<double>& exscore)
{
	// GPMC uses the lexicographical order of var_frequency and activity for choosing a decision var.
	// ToDo: try other heuristics

	Component& c = topComponent();
	assert(dl_.back().SplitCompsEnd() == comp_stack_.size());

	assert(c.nVars() > 0);
	Var maxv = var_Undef;
	double max_score_a = -1;
	double max_score_f = -1;

	if(config.varSelectionHueristics == 0) {
		int p;
		for(p=0; isPVar(c[p]) && c[p] != var_Undef; p++) {
			double score_f = var_frequency_[c[p]] + exscore[c[p]];
			double score_a = activity[c[p]];
			if( score_f > max_score_f) {
				max_score_f = score_f;
				max_score_a = score_a;
				maxv = c[p];
				//		} else if ( score_f == max_score_f && score_a > max_score_a) {
			} else if ( score_f >= max_score_f * 0.90 && score_a > max_score_a * 1.5) {
				max_score_a = score_a;
				maxv = c[p];
			}
		}
	} else {
		double max_score_td = -1;
		int p;
		for(p=0; isPVar(c[p]) && c[p] != var_Undef; p++) {
			double score_td = exscore[c[p]];
			double score_f = var_frequency_[c[p]];
			double score_a = activity[c[p]];

			if(score_td > max_score_td) {
				max_score_td = score_td;
				max_score_f = score_f;
				max_score_a = score_a;
				maxv = c[p];
			}
			else if( score_td == max_score_td) {
				if(score_f > max_score_f) {
					max_score_f = score_f;
					max_score_a = score_a;
					maxv = c[p];
				} else if (score_f == max_score_f && score_a > max_score_a) {
					max_score_a = score_a;
					maxv = c[p];
				}
			}
		}
	}
	assert(maxv != var_Undef);
	return maxv;
}

template <class T_data>
void ComponentManager<T_data>::removeCachePollutions() {
	assert(topDecision().baseComp() == comp_stack_.size()-1);
	cache_.cleanAllDescendantsOf(comp_stack_.back()->id());
	// cache_.cleanPollutionsInvolving(comp_stack_.back()->id());
}

template <class T_data>
void ComponentManager<T_data>::setDecisionNode(Lit l)
{
	NodeIndex n = nodeMgr.newNode(DT_AND);
	nodeMgr.addEdge(n, nodeMgr.Literal(l));
	dl_.back().setNodeIdx(n);
}
template <class T_data>
void ComponentManager<T_data>::addNode(NodeIndex x, NodeIndex to)
{
	assert(x != nullptr && !nodeMgr.isTop(x));

	NodeIndex n = to;

	if(x == BOTTOM_NODE) {
		nodeMgr.deleteNodes(n);
		dl_.back().setNodeIdx(BOTTOM_NODE);
	}
	else {
		nodeMgr.addEdge(n, x);
	}
}
template <class T_data>
NodeIndex ComponentManager<T_data>::makeBranchNode(Var x)
{
	NodeIndex nodes[2];
	for(auto branch : {0, 1}) {
		NodeIndex n = nodes[branch] = dl_.back().getBranchNodeIdx(branch);

		if(n != BOTTOM_NODE) {
			DTNode& v = nodeMgr.Node(n);
			assert(v.Type() == DT_AND);
			if(v.Children().size() == 1) {
				// remove a trivial AND Node
				nodes[branch] = v.Children()[0];
				nodeMgr.delEdge(n, v.Children()[0]);
				nodeMgr.removeNode(n);
			}
		}
	}

	return nodeMgr.OR(x, nodes[0], nodes[1]);
}
template <class T_data>
void ComponentManager<T_data>::removeChildComponentNodes()
{
	NodeIndex node = dl_.back().getNodeIdx();
	vector<NodeIndex>& children = nodeMgr.Node(node).Children();
	for(int i=children.size()-1; i>=0; i--) {
		DT_NodeType t = nodeMgr.Node(children[i]).Type();
		if(t == DT_AND || t == DT_OR || t == DT_DC) {
			std::swap(children[i], children.back());
			nodeMgr.delEdge(node, children.back());
			if(nodeMgr.Node(children.back()).Parents().size() == 0) {
				nodeMgr.deleteNodes(children.back());
				children.pop_back();
			}
		}
	}
}

template class GPMC::ComponentManager<mpz_class>;
