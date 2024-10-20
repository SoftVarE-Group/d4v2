#ifndef DECISIONTREE_H
#define DECISIONTREE_H

#include <gmpxx.h>
#include <iostream>
#include <vector>
#include <stack>
#include <queue>

#include "core/SolverTypes.h"

using namespace std;

class DTNode;
typedef DTNode* NodeIndex;

#define UNDEF_NODE nullptr
#define UNDEF_VAL -1

enum DT_NodeType : char
{
	DT_TOP,
	DT_BOT,
	DT_LIT,
	DT_AND,
	DT_OR,
	DT_DC
};

class DTNodeManager;

class DTNode
{
	DT_NodeType type;
	char state;
	unsigned int id;
	int val;			/* 	When DT_OR, this value is x+1 where x is a decision var.
	 	 	 	 	 	 	When DT_LIt, this value is x+1(or -(x+1)) for a positive (or negative) of var x. */
	vector<NodeIndex> children;
	vector<NodeIndex> parents;

public:
	DTNode() :
		type(DT_AND), state(0), id(UINT_MAX), val(0)
	{ }
	DTNode(DT_NodeType type, int arg=0) :
		type(type), state(0), id(UINT_MAX), val(arg)
	{ }

	~DTNode() { parents.clear(); children.clear(); }

	DT_NodeType Type() const { return type; }
	char State() const { return state; }
	unsigned int Id() const { return id; }
	int DecisionVar() const { assert(Type() == DT_OR || Type() == DT_DC); return val; }
	int Literal() const { assert(Type() == DT_LIT); return val; }

	void setType(DT_NodeType type) { this->type = type; }
	void setState(char state) { this->state = state; }
	void setId(unsigned int id) { this->id = id; }
	void reset(DT_NodeType type, int arg) { this->type = type; this->val = arg; }

	vector<NodeIndex>& Children() { return children; }
	vector<NodeIndex>& Parents() { return parents; }
	void addParent(NodeIndex parent) { parents.push_back(parent); }
	void addChild(NodeIndex child) { children.push_back(child); }
	void delParent(NodeIndex parent);
	void delChild(NodeIndex child);
};
inline void DTNode::delParent(NodeIndex parent)
{
	auto it = std::find(parents.rbegin(), parents.rend(), parent);
	assert(it != parents.rend());
	parents.erase((++it).base());
}
inline void DTNode::delChild(NodeIndex child)
{
	auto it = std::find(children.rbegin(), children.rend(), child);
	assert(it != children.rend());
	children.erase((++it).base());
}
extern DTNode topnode;
extern DTNode bottomnode;
const NodeIndex TOP_NODE = &topnode;
const NodeIndex BOTTOM_NODE = &bottomnode;

const size_t default_blocksize = (1 << 14);
const size_t max_blocksize = (1 << 22);
struct NodeBlock {
	NodeIndex ptr;
	size_t cap;		// capacity
	size_t sz;			// size, including recycled ones

	NodeBlock(NodeIndex ptr, size_t cap) : ptr(ptr), cap(cap), sz(0) { }
	bool isFull() { return sz >= cap; }
	NodeIndex getNode() { NodeIndex p = &(ptr[sz]); sz++; return p; }
};

class DTNodeManager {
	NodeIndex basicnodes;
	size_t poolnodes;
	std::vector<NodeBlock> pool;
	std::queue<NodeIndex> recycles;

	NodeIndex root;
	char state;

	uint64_t nodes;		/* the number of internal nodes */
	uint64_t edges;
	int vars;

public:
	DTNodeManager() : basicnodes(UNDEF_NODE), poolnodes(0), root(UNDEF_NODE), state(0), nodes(0), edges(0), vars(0) { }
	~DTNodeManager() {
		for(auto bk : pool)
			delete[] bk.ptr;
		delete[] basicnodes;
	}

	NodeIndex Root() { return root; }
	void setRoot(NodeIndex node) { this->root = node; }

	NodeIndex newNode(DT_NodeType type, int arg=UNDEF_VAL);
	DTNode& Node(NodeIndex node) { return *node; }
	void removeNode(NodeIndex node);	/* remove only the node */
	void deleteNodes(NodeIndex node);	/* delete the node and its descendants that do not have another ancestor, not a descendant of the node) */

	void addEdge(NodeIndex parent, NodeIndex child);
	void delEdge(NodeIndex parent, NodeIndex child);

	NodeIndex Bottom() const { return BOTTOM_NODE; }
	NodeIndex Top() const { return TOP_NODE; }
	bool isBottom(NodeIndex node) const { return node == BOTTOM_NODE; }
	bool isTop(NodeIndex node) const { return node == TOP_NODE; }

	void init(int vars);
	/* This class reserves nodes for literals and Don't-cares of all variables when initialization. */

	NodeIndex Literal(Glucose1::Lit l) const { return basicnodes + toInt(l); }
	NodeIndex Literal(Glucose1::Var x, bool sign) { return basicnodes + toInt(Glucose1::mkLit(x, sign)); }
	NodeIndex DontCare(Glucose1::Var x) { return basicnodes + ((vars << 1) + x); }
	NodeIndex getDCNode(Glucose1::Var x);

	NodeIndex OR(Glucose1::Var x, NodeIndex first, NodeIndex second);
	void compressANDs();

	template <class T_data> T_data countModel(Glucose1::vec<T_data>& lit_weight, bool wc);

	void printNode(NodeIndex node, ostream& out) const;
	void printNNF(ostream& out);
	void printStats() const;
};

#endif
