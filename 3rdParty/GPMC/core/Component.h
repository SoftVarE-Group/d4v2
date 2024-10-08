#ifndef COMPONENT_H_
#define COMPONENT_H_

#include "mtl/Alloc.h"
#include "core/SolverTypes.h"
#include "core/Config.h"
#include "ddnnf/DecisionTree.h"

#include <gmpxx.h>
#include <vector>
#include <math.h>

using namespace Glucose1;
using namespace std;

namespace GPMC {

typedef int ClID;
#define clid_Undef (-1)

typedef unsigned CacheEntryID;

template <class T_data>
class Decision {
	int trail_pos_;
	bool cur_branch_;

	unsigned basecomp_;
	unsigned splitcompFrom_;	// [From, End)
	unsigned splitcompEnd_;

	T_data models_[2];
	bool hasmodel_[2];

	T_data branch_weight_;
	NodeIndex nodes[2];

public:
	Decision(int trail_pos, unsigned basecomp) :
		trail_pos_(trail_pos), cur_branch_(false), basecomp_(basecomp),
		splitcompFrom_(basecomp + 1), splitcompEnd_(basecomp + 1), models_{ 0, 0 }, hasmodel_{false, false} {
		branch_weight_ = 1;
		nodes[0] = nodes[1] = BOTTOM_NODE;
	}

	int trailPos() {
		return trail_pos_;
	}
	unsigned baseComp() {
		return basecomp_;
	}

	//
	bool isFirstBranch() {
		return !cur_branch_;
	}
	void changeBranch() {
		cur_branch_ = true;
	}

	//
	unsigned SplitCompsEnd() {
		return splitcompEnd_;
	}
	bool hasUnprocessedSplitComp() {
		return splitcompFrom_ < splitcompEnd_;
		// If true, the top of comp_stack is an unprocessed split component in this DL.
	}

	void setSplitCompsEnd(int end) {
		splitcompEnd_ = end;
	}
	void nextSplitComp() {
		splitcompEnd_--;
	}
	int numUnprocessedSplitComp() {
		return splitcompEnd_ - splitcompFrom_;
	}

	// Methods on models
	const T_data totalModels() const {
		return models_[0] + models_[1];
	}

	const bool hasModel() const {
		return hasmodel_[false] || hasmodel_[true];
	}

	const T_data currentBranchModels() const {
		return models_[cur_branch_];
	}

	bool isUnSAT() {
		// return models_[cur_branch_] == 0;
		return !hasmodel_[cur_branch_];
	}

	void increaseModels(const T_data &nmodels, bool found = true) {
		if(found) {
			if(hasmodel_[cur_branch_])
				models_[cur_branch_] *= nmodels;
			else
				models_[cur_branch_] = nmodels;
		} else {
			models_[cur_branch_] = 0;
		}
		hasmodel_[cur_branch_] = found;
	}

	void setBranchWeight(const T_data &w) {
		branch_weight_ = w;
	}
	void mulBranchWeight(const T_data &w) {
		// cout << w << endl;
		branch_weight_ *= w;
	}
	void mulLitsWeights() {
		if(hasmodel_[cur_branch_])
			models_[cur_branch_] *= branch_weight_;
	}

	NodeIndex getBranchNodeIdx(int branch) {
		return this->nodes[branch];
	}
	NodeIndex getNodeIdx() {
		return this->nodes[cur_branch_];
	}
	void setNodeIdx(NodeIndex node) {
		this->nodes[cur_branch_] = node;
	}

	vector<unsigned> dec_cands;
};

class Component {
	CacheEntryID id_ = 0;
	int nvars_;		// The number of variables in this component
	int ncls_;		// The number of long clauses (of size > 2) in this component

	bool hasPVar_;	// Whether this component has a projection var or not.

	vec<int> data_;	// [vars ..] @ [var_Undef] @ [clids ..] @ [clid_Undef]

public:
	Component(int nvars, int ncls, bool hasPVar) :
		hasPVar_(hasPVar) {
		capacity(nvars, ncls);
	}

	void capacity(int nvars, int ncls) {
		nvars_ = nvars;
		ncls_ = ncls;
		data_.capacity(nvars + ncls + 2);
	}

	// Store
	void pushVar(Var v) {
		assert(data_.size() <= nvars_);
		data_.push_(v);
	}
	void closeVar() {
		assert(data_.size() == nvars_);
		data_.push_(var_Undef);
	}
	void pushClID(ClID cid) {
		assert(data_.size() > nvars_);
		data_.push_(cid);
	}
	void closeClID() {
		assert(data_.size() == nvars_ + ncls_ + 1);
		data_.push_(clid_Undef);
	}

	// Access
	int nVars() {
		return nvars_;
	}
	int nlongCls() {
		return ncls_;
	}
	int& operator [](int i) {
		assert(data_[nvars_] == var_Undef);
		return data_[i];
	}
	int operator [](int i) const {
		assert(data_[nvars_+ncls_+1] == clid_Undef);
		return data_[i];
	}

	bool hasPVar() {
		return hasPVar_;
	}
	int nPVars(int npvars) {
		int num = 0;
		for(; data_[num] != var_Undef && data_[num] < npvars; num++);
		return num;
	}

	void set_id(CacheEntryID id) {
		id_ = id;
	}
	CacheEntryID id() {
		return id_;
	}

};

template <class T_data>
class PackedComponent {
private:
	static unsigned _bits_per_clause;
	static unsigned _bits_per_variable;
	static unsigned _variable_mask;
	static unsigned _clause_mask;
	static const unsigned _bitsPerBlock = (sizeof(unsigned) << 3);

public:
	static unsigned bits_per_variable() {
		return _bits_per_variable;
	}
	static unsigned bits_per_clause() {
		return _bits_per_clause;
	}

	static void adjustPackSize(unsigned int maxVarId, unsigned int maxClId);

	// Field

protected:
	unsigned* data_ = nullptr;
	unsigned clauses_ofs_ = 0;
	unsigned hashkey_ = 0;

	T_data model_count_;
	unsigned creation_time_ = 0;

	NodeIndex node_;

public:
	PackedComponent() : node_(UNDEF_NODE){
	}

	inline PackedComponent(Component& rComp);

	PackedComponent(Component & rComp, const T_data &model_count, unsigned long time, NodeIndex node) :
		PackedComponent(rComp) {
		model_count_ = model_count;
		creation_time_ = time;
		node_ = node;
	}

	~PackedComponent() {
		if (data_)
			free(data_);
	}

	unsigned data_size() const {
		if (!data_)
			return 0;

		unsigned *p = data_;
		while (*p)
			p++;
		return (p - data_ + 2); // return (p - data_ + 1);
	}

	unsigned creation_time() {
		return creation_time_;
	}
	void set_creation_time(unsigned time) {
		creation_time_ = time;
	}

	const T_data & model_count() const {
		return model_count_;
	}
	void set_model_count(const T_data &rn) {
		model_count_ = rn;
	}

	NodeIndex get_DTNode() const {
		return node_;
	}
	void set_DTNode(NodeIndex node) {
		node_ = node;
	}

	unsigned hashkey() {
		return hashkey_;
	}

	// NOTE that the following is only an upper bound on
	// the number of variables
	// it might overcount by a few variables
	// this is due to the way things are packed
	// and to reduce time needed to compute this value
	unsigned num_variables() {
		unsigned bits_per_var_diff = (*data_) & 31;
		return 1
				+ (clauses_ofs_ * sizeof(unsigned) * 8 - bits_per_variable() - 5)
				/ bits_per_var_diff;
	}
	//  unsigned num_variables() {
	//      return (clauses_ofs_ * sizeof(unsigned) * 8) /_bits_per_variable;
	//  }

	inline bool equals(const PackedComponent &comp) const;

};

template <class T_data>
class CachedComponent: public PackedComponent<T_data> {

	// the position where this
	// component is stored in the component stack
	// if this is non-zero, we may not simply delete this
	// component
	unsigned component_stack_id_ = 0;

	// theFather and theDescendants:
	// each CCacheEntry is a Node in a tree which represents the relationship
	// of the components stored
	CacheEntryID father_ = 0;
	CacheEntryID first_descendant_ = 0;
	CacheEntryID next_sibling_ = 0;

public:
	CachedComponent() {
	}

	CachedComponent(Component &comp, const T_data &model_count, unsigned long time, NodeIndex node) :
		PackedComponent<T_data>(comp, model_count, time, node) {
	}

	CachedComponent(Component &comp) :
		PackedComponent<T_data>(comp) {
	}

	// a cache entry is deletable
	// only if it is not connected to an active
	// component in the component stack
	bool deletable() {
		return component_stack_id_ == 0;
	}
	void eraseComponentStackID() {
		component_stack_id_ = 0;
	}
	void setComponentStackID(unsigned id) {
		component_stack_id_ = id;
	}
	unsigned component_stack_id() {
		return component_stack_id_;
	}

	void clear() {
		// before deleting the contents of this component,
		// we should make sure that this component is not present in the component stack anymore!
		assert(component_stack_id_ == 0);
		if (this->data_)
			delete this->data_;
		this->data_ = nullptr;
	}
	unsigned long SizeInBytes() const {
		return sizeof(CachedComponent)
				+ PackedComponent<T_data>::data_size() * sizeof(unsigned)
				+ sizeof(T_data);
				// ToDo: How to estimate the actual data size of mpz_class
	}

	void set_father(CacheEntryID f) {
		father_ = f;
	}
	void set_next_sibling(CacheEntryID sb) {
		next_sibling_ = sb;
	}
	void set_first_descendant(CacheEntryID ds) {
		first_descendant_ = ds;
	}

	const CacheEntryID father() const {
		return father_;
	}
	CacheEntryID next_sibling() {
		return next_sibling_;
	}
	CacheEntryID first_descendant() {
		return first_descendant_;
	}
};

template <class T_data> class ComponentCache;

template <class T_data>
class CacheBucket: protected vector<CacheEntryID> {
	friend class ComponentCache<T_data>;

public:

	using vector<CacheEntryID>::size;

	unsigned long getBytesMemoryUsage() {
		return sizeof(CacheBucket) + size() * sizeof(CacheEntryID);
	}
};

template <class T_data>
class ComponentCache {
	vector<CachedComponent<T_data> *> entry_base_;
	vector<CacheEntryID> free_entry_base_slots_;

	// the actual hash table
	// by means of which the cache is accessed
	vector<CacheBucket<T_data> *> table_;

	//	  SolverConfiguration &config_;
	//	  DataAndStatistics &statistics_;

	unsigned long num_occupied_buckets_ = 0;
	unsigned long my_time_ = 0;

public:
	void init(int cachesize);

	void add_descendant(CacheEntryID compid, CacheEntryID descendantid) {
		assert(descendantid != entry(compid).first_descendant());
		entry(descendantid).set_next_sibling(entry(compid).first_descendant());
		entry(compid).set_first_descendant(descendantid);
	}
#if 0
	void remove_firstdescendantOf(CacheEntryID compid) {
		CacheEntryID desc = entry(compid).first_descendant();
		if (desc != 0)
			entry(compid).set_first_descendant(entry(desc).next_sibling());
	}
#endif
	unsigned long used_memory_MB() {
		return recompute_bytes_memory_usage() / 1000000;
	}

	//  ComponentCache(SolverConfiguration &conf, DataAndStatistics &statistics);

	~ComponentCache() {
		for (auto &pbucket : table_)
			if (pbucket != nullptr)
				delete pbucket;
		for (auto &pentry : entry_base_)
			if (pentry != nullptr)
				delete pentry;
	}

	CachedComponent<T_data> &entry(CacheEntryID id) {
		assert(entry_base_.size() > id);
		assert(entry_base_[id] != nullptr);
		return *entry_base_[id];
	}

	bool hasEntry(CacheEntryID id) {
		assert(entry_base_.size() > id);
		return entry_base_[id];
	}

	// creates a CCacheEntry in the entry base
	// which contains a packed copy of comp
	// returns the id of the entry created
	// stores in the entry the position of
	// comp which is a part of the component stack
	CacheEntryID createEntryFor(Component &comp, unsigned stack_id);

	// unchecked erase of an entry from entry_base_
	void eraseEntry(CacheEntryID id) {
		cache_bytes_memory_usage_ -= entry_base_[id]->SizeInBytes();
		sum_size_cached_components_ -= entry_base_[id]->num_variables();
		num_cached_components_--;
		delete entry_base_[id];
		entry_base_[id] = nullptr;
		free_entry_base_slots_.push_back(id);
	}

	// store the number in model_count as the model count of CacheEntryID id
	inline void storeValueOf(CacheEntryID id, const T_data &model_count, NodeIndex node=UNDEF_NODE);

	// check if the cache contains the modelcount of comp
	// if so, return true and out_model_count contains that model count
	bool requestValueOf(Component &comp, T_data &out_model_count, NodeIndex &node);

	bool deleteEntries();
	void deleteallentries();

	// test function to ensure consistency of the descendant tree
	// inline void test_descendantstree_consistency();

	// --- Added by k-hasimt --- BEGIN
	inline void cleanAllDescendantsOf(CacheEntryID id);

	void printStats() const {
		printf("c o Cache lookup          = %"PRIu64"\n", num_cache_look_ups_);
		printf("c o Cache hit             = %"PRIu64"\n", num_cache_hits_);
		printf("c o Cache reduce          = %"PRIu64"\n", num_cache_reduce_);
	}
	// --- Added by k-hasimt --- END

private:
	uint64_t num_cache_hits_ = 0;
	uint64_t num_cache_look_ups_ = 0;
	uint64_t num_cache_reduce_ = 0;
	uint64_t sum_cache_hit_sizes_ = 0;
	uint64_t num_cached_components_ = 0;
	uint64_t sum_size_cached_components_ = 0;
	// uint64_t sum_memory_size_cached_components_ = 0;
	uint64_t cache_bytes_memory_usage_ = 0;
	uint64_t maximum_cache_size_bytes;

	// compute the size in bytes of the component cache from scratch
	// the value is stored in bytes_memory_usage_
	uint64_t recompute_bytes_memory_usage();

	// removes the entry id from the hash table
	// but not from the entry base
	inline void removeFromHashTable(CacheEntryID id);

	// we delete the Component with ID id
	// and all its descendants from the cache
	inline void cleanPollutionsInvolving(CacheEntryID id);

	// delete entries, keeping the descendants tree consistent
	inline void removeFromDescendantsTree(CacheEntryID id);

	unsigned int clip(long unsigned int ofs) {
		return ofs % table_.size();
	}

	CacheBucket<T_data> &at(unsigned int ofs) {
		if (table_[ofs] == NULL) {
			num_occupied_buckets_++;
			table_[ofs] = new CacheBucket<T_data>();
		}
		return *table_[ofs];
	}

	bool isBucketAt(unsigned int ofs) {
		return table_[ofs] != NULL;
	}
};

template <class T_data>
class ComponentManager {
	enum scStateT {
		SC_NOT_CANDIDATE, SC_CANDIDATE, SC_SEEN, SC_DONE
	};

	//
	ClauseAllocator ca_;
	vec<CRef> clauses_;
	vec<uint32_t> occlists_;
	vec<int> occ_pool_;

	vec<scStateT> varseen_;
	vec<scStateT> clseen_;

	vec<double> var_frequency_;

	// double cur_max_frequency_;

	int npvars_;
	uint64_t components;
	uint64_t num_try_split;

	vector<Decision<T_data>> dl_;
	vector<Component*> comp_stack_;

	ComponentCache<T_data> cache_;
	DTNodeManager nodeMgr;

	ConfigComponentManager config;

	int fixed_level_;

public:
	ComponentManager() :
		npvars_(0), components(0), num_try_split(0), fixed_level_(0) {
	}

	~ComponentManager()
	{
		while(comp_stack_.size() > 0){
			delete comp_stack_.back();
			comp_stack_.pop_back();
		}
	}

	void setConfig(ConfigComponentManager cfg) { this->config = cfg; }
	void init(int nvars, int npvars, const vec<CRef>& sclauses, const ClauseAllocator& sca);

	int splitComponent(const vec<lbool>& assigns, const vec<T_data>& lit_weight = {});

	Var pickBranchVar(const vec<double>& activity, const vec<double>& exscore);

	// Decision Stack
	Decision<T_data>& topDecision() {
		return dl_.back();
	}
	Decision<T_data>& prevDecision() {
		return *(dl_.end() - 2);
	}
	void pushDecision(int trailp) {
		dl_.push_back(Decision<T_data>(trailp, comp_stack_.size() - 1));
	}
	void popDecision() {
		dl_.pop_back();
	}

	void setDecCand() {
		vector<unsigned>& dec_cands = dl_.back().dec_cands;
		dec_cands.resize(npvars_+1);
		dec_cands[npvars_]++;

		Component& c = topComponent();
		int p;
		for(p=0; isPVar(c[p]) && c[p] != var_Undef; p++) {
			dec_cands[c[p]] = dec_cands[npvars_];
		}
	}
	bool isDecCand(Var x) {
		return dl_.back().dec_cands[x] == dl_.back().dec_cands[npvars_];
	}

	// Component Stack
	Component& topComponent() {
		return *(comp_stack_.back());
	}
	void popComponent() {
		//assert(dl_.back().hasUnprocessedSplitComp());

		delete comp_stack_.back();
		comp_stack_.pop_back();
		dl_.back().nextSplitComp();
	}

	ComponentCache<T_data> &cache() {
		return cache_;
	}

	void cacheModelCountOf(unsigned stack_comp_id, const T_data &value, NodeIndex node) {
		//if (config_.perform_component_caching && (config_.perform_component_caching_in_sat || !isNotCounting()))
		cache_.storeValueOf(stack_comp_id, value, node);
	}
	void removeCachePollutions();
	void eraseComponentStackID() {
		if (cache_.hasEntry(comp_stack_.back()->id()))
			cache_.entry(comp_stack_.back()->id()).eraseComponentStackID();
	}

	void backjumpTo(int level) {
		while (dl_.size() > (unsigned) level + 1) {
			if(config.ddnnf)
				for(auto i : {0,1})
					nodeMgr.deleteNodes(dl_.back().getBranchNodeIdx(i));
			popDecision();
		}
		while (comp_stack_.size() > dl_.back().baseComp() + 1)
			popComponent();
		dl_.back().setSplitCompsEnd(comp_stack_.size());
	}

	// Debug
	void printDimacs(const vec<CRef>& clauses, const ClauseAllocator& ca, const vec<lbool>& assign);

	void printStats() const {
		printf("c o Components            = %"PRIu64"\n", components);
		printf("c o Split                 = %"PRIu64"\n", num_try_split);
		cache_.printStats();
	}

	int searchNewLimLevel() {
		int level = dl_.size() - 1;
		while (level > 0 && dl_[level].totalModels() == 0 && dl_[level].numUnprocessedSplitComp()==1)
			//while (level > 0 && dl_[level].totalModels() == 0)
			level--;
		return level + 1;
	}

	void printScore(Var v, double activity) {
		printf("c o activity:%lf, var_freq:%lf\n", activity, var_frequency_[v]);
	}

	bool checkfixedDL() {
		if(fixed_level_+1 == dl_.size()) {
			if(!dl_.back().isFirstBranch() && dl_.back().numUnprocessedSplitComp()==1) {
				fixed_level_++;
				return true;
			}
		}
		return false;
	}

	void setRoot(NodeIndex node) { nodeMgr.setRoot(node); }
	void setDecisionNode(Lit l);
	void addNode(NodeIndex node) { addNode(node, dl_.back().getNodeIdx()); }
	void addNode(NodeIndex node, NodeIndex to);
	void addLitNode(Lit l) { addNode(nodeMgr.Literal(l)); }
	NodeIndex makeBranchNode(Var x);
	void removeChildComponentNodes();
	DTNodeManager& NodeManager() { return nodeMgr; }
	const DTNodeManager& NodeManagerConst() const { return nodeMgr; }

protected:
	bool isPVar(Var v) {
		return v < npvars_;
	}

	double scoreOf(Var v, double activity) {
		return var_frequency_[v];
	}

	lbool value(Var x, const vec<lbool>& assigns) const {
		return assigns[x];
	}	// The current value of a variable.

	lbool value(Lit p, const vec<lbool>& assigns) const {
		return assigns[var(p)] ^ sign(p);
	}    // The current value of a literal.

	void initComponentStack(int nvars, int ncls);
	void initDecisionStack();

	void searchComponent(Var seed_var, const vec<lbool>& assigns,
			int& nvar_in_comp, int& ncls_in_comp);
};

template <class T_data>
bool PackedComponent<T_data>::equals(const PackedComponent<T_data> &comp) const {
	if (clauses_ofs_ != comp.clauses_ofs_)
		return false;

	unsigned* p = data_;
	unsigned* r = comp.data_;
	while ((*p || *(p + 1)) && *p == *r) {		// modified by k-hasimt. One more 0 is added in the end of a packed component.
		p++;
		r++;
	}
	return *p == *r && *(p + 1) == *(r + 1);
}

template <class T_data>
PackedComponent<T_data>::PackedComponent(Component &rComp) {
	int max_diff = 0;
	Var v1, v2;
	ClID c1, c2;
	int i;

	// Calculate the length of bits (*1) to represent difference of ids of adjacent vars
	v2 = rComp[0];
	for (i = 1; (v1 = rComp[i]) != var_Undef; i++) {
		int diff = v1 - v2;
		if (diff > max_diff)
			max_diff = diff;
		v2 = v1;
	}
	unsigned bits_per_var_diff = (unsigned int) ceil(log((double) max_diff + 1) / log(2.0));
	if (bits_per_var_diff == 0)
		bits_per_var_diff = 1;
	assert(bits_per_var_diff != 0);
	assert((bits_per_var_diff & 31) != 0);
	assert(bits_per_var_diff <= _bits_per_variable);

	// Calculate the length of bits (*2) to represent difference of ids of adjacent clauses
	max_diff = 1;
	unsigned bits_per_clause_diff = 0;
	if (rComp.nlongCls()) {
		i = rComp.nVars() + 1;
		c2 = rComp[i];
		for (i++; (c1 = rComp[i]) != clid_Undef; i++) {
			int diff = c1 - c2;
			if (diff > max_diff)
				max_diff = diff;
			c2 = c1;
		}
		bits_per_clause_diff = (unsigned int) ceil(log((double) max_diff + 1) / log(2.0));
	}
	assert(bits_per_clause_diff <= _bits_per_clause);

	// data_ : a bit sequence, packed in unsigned(32bit) containers,
	// [length (*1) (5bits)][First var id][diffseq...][0][length (*2)][1st clauseID][diffseq...][0][0] (rough sketch of content)
	unsigned data_size = (_bits_per_variable + 5 + _bits_per_clause + 5
			+ (rComp.nVars() - 1) * bits_per_var_diff
			+ (rComp.nlongCls() - 1) * bits_per_clause_diff) / _bitsPerBlock
					+ 4;

	unsigned * p = data_ = (unsigned*) malloc(sizeof(unsigned) * data_size);
	if(p == NULL)
		throw OutOfMemoryException();

	unsigned int bitpos;

	*p = bits_per_var_diff;
	bitpos = 5;
	*p |= rComp[0] << bitpos;
	bitpos += _bits_per_variable;

	unsigned hashkey_vars = rComp[0];

	v2 = rComp[0];
	for (i = 1; (v1 = rComp[i]) != var_Undef; i++) {
		*p |= ((unsigned) (v1 - v2)) << bitpos;
		bitpos += bits_per_var_diff;
		hashkey_vars = hashkey_vars * 3 + (v1 - v2);
		if (bitpos >= _bitsPerBlock) {
			bitpos -= _bitsPerBlock;
			*(++p) = ((v1 - v2) >> (bits_per_var_diff - bitpos));
		}
		v2 = v1;
	}
	if (bitpos > 0)
		p++;

	clauses_ofs_ = p - data_; // starting the clause part

	unsigned hashkey_clauses = 0;
	if (rComp.nlongCls()) {
		*p = bits_per_clause_diff;
		bitpos = 5;

		*p |= rComp[rComp.nVars() + 1] << bitpos;
		bitpos += _bits_per_clause;

		i = rComp.nVars() + 1;
		c2 = rComp[i];
		for (i++; (c1 = rComp[i]) != clid_Undef; i++) {
			*p |= ((unsigned) (c1 - c2)) << (bitpos);
			bitpos += bits_per_clause_diff;
			hashkey_clauses = hashkey_clauses * 3 + (c1 - c2);
			if (bitpos >= _bitsPerBlock) {
				bitpos -= _bitsPerBlock;
				*(++p) = ((c1 - c2) >> (bits_per_clause_diff - bitpos));
			}
			c2 = c1;
		}
		if (bitpos > 0)
			p++;
	}
	*p = 0;
	*(p + 1) = 0;	// added by k-hasimt
	hashkey_ = hashkey_vars + (((unsigned long) hashkey_clauses) << 16);

	node_ = UNDEF_NODE;
}

template <class T_data>
void ComponentCache<T_data>::cleanPollutionsInvolving(CacheEntryID id) {
	CacheEntryID father = entry(id).father();
	if (entry(father).first_descendant() == id) {
		entry(father).set_first_descendant(entry(id).next_sibling());
	} else {
		CacheEntryID act_sibl = entry(father).first_descendant();
		while (act_sibl) {
			CacheEntryID next_sibl = entry(act_sibl).next_sibling();
			if (next_sibl == id) {
				entry(act_sibl).set_next_sibling(
						entry(next_sibl).next_sibling());
				break;
			}
			act_sibl = next_sibl;
		}
	}
	CacheEntryID next_child = entry(id).first_descendant();
	entry(id).set_first_descendant(0);
	while (next_child) {
		CacheEntryID act_child = next_child;
		next_child = entry(act_child).next_sibling();
		cleanPollutionsInvolving(act_child);
	}
	removeFromHashTable(id);
	eraseEntry(id);
}

template <class T_data>
void ComponentCache<T_data>::removeFromHashTable(CacheEntryID id) {
	unsigned int v = clip(entry(id).hashkey());
	if (isBucketAt(v))
		for (auto it = table_[v]->begin(); it != table_[v]->end(); it++) {
			if (*it == id) {
				*it = table_[v]->back();
				table_[v]->pop_back();
				break;
			}
		}
}

template <class T_data>
void ComponentCache<T_data>::removeFromDescendantsTree(CacheEntryID id) {
	assert(hasEntry(id));
	// we need a father for this all to work
	assert(entry(id).father());
	assert(hasEntry(entry(id).father()));


	CacheEntryID father = entry(id).father();
	if (entry(father).first_descendant() == id) {
		entry(father).set_first_descendant(entry(id).next_sibling());
	} else {
		CacheEntryID act_sibl = entry(father).first_descendant();
		while (act_sibl) {
			CacheEntryID next_sibl = entry(act_sibl).next_sibling();
			if (next_sibl == id) {
				entry(act_sibl).set_next_sibling(
						entry(next_sibl).next_sibling());
				break;
			}
			act_sibl = next_sibl;
		}
	}

	CacheEntryID act_child = entry(id).first_descendant();
	while (act_child) {
		CacheEntryID next_child = entry(act_child).next_sibling();
		entry(act_child).set_father(father);
		entry(act_child).set_next_sibling(entry(father).first_descendant());
		entry(father).set_first_descendant(act_child);
		act_child = next_child;
	}
}

template <class T_data>
void ComponentCache<T_data>::storeValueOf(CacheEntryID id, const T_data &model_count, NodeIndex node) {
	CacheBucket<T_data> &bucket = at(clip(entry(id).hashkey()));
	// when storing the new model count the size of the model count
	// and hence that of the component will change
	cache_bytes_memory_usage_ -= entry(id).SizeInBytes();

	entry(id).set_model_count(model_count);
	entry(id).set_creation_time(my_time_);
	entry(id).set_DTNode(node);
	bucket.push_back(id);
	cache_bytes_memory_usage_ += entry(id).SizeInBytes();
}

// --- Added by k-hasimt --- BEGIN
template <class T_data>
void ComponentCache<T_data>::cleanAllDescendantsOf(CacheEntryID id) {
	CacheEntryID next_child = entry(id).first_descendant();
	while (next_child) {
		CacheEntryID act_child = next_child;
		next_child = entry(act_child).next_sibling();
		cleanPollutionsInvolving(act_child);
	}
}
// --- Added by k-hasimt --- END

}

#endif /* COMPONENT_H_ */
