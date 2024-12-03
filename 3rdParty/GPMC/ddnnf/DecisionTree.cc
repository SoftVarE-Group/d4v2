#include "DecisionTree.h"

DTNode topnode(DT_TOP);
DTNode bottomnode(DT_BOT);

// DTManager
void DTNodeManager::init(int npvars)
{
	NodeIndex p = basicnodes = new DTNode[3*npvars];

	for(int i=0; i<npvars; i++) {
		p->reset(DT_LIT, i+1); p++;
		p->reset(DT_LIT, -(i+1)); p++;
	}
	for(int i=0; i<npvars; i++) {
		p->reset(DT_DC, i+1); p++;
	}
	vars = npvars;

	pool.clear();
	NodeIndex fb = new DTNode[default_blocksize];
	pool.push_back(NodeBlock(fb, default_blocksize));
	poolnodes = default_blocksize;
}
NodeIndex DTNodeManager::newNode(DT_NodeType type, int arg)
{
    NodeIndex p;

    if(recycles.empty()) {
    	NodeBlock& b = pool.back();
    	if(b.isFull()) {
    		size_t next_blkcap = (poolnodes < max_blocksize) ? poolnodes : max_blocksize;
    		NodeIndex newp = new DTNode[next_blkcap];
    		pool.push_back(NodeBlock(newp, next_blkcap));
    		poolnodes += next_blkcap;
    		p = pool.back().getNode();
    	} else {
    		p = b.getNode();
    	}
    	p->reset(type, arg);
    }
    else {
    	p = recycles.front();
    	recycles.pop();
    	p->reset(type, arg);
    }
    nodes++;

    return p;
}
void DTNodeManager::removeNode(NodeIndex node)
{
	assert(node != nullptr);

	assert(node->Children().size() == 0 && node->Parents().size() == 0);

	if(node->Type() == DT_LIT) return;

	if(node->Type() != DT_DC) {
		recycles.push(node);
	}

	nodes--;
}
void DTNodeManager::addEdge(NodeIndex parent, NodeIndex child)
{
	parent->addChild(child);
	child->addParent(parent);
	edges++;
}
void DTNodeManager::delEdge(NodeIndex parent, NodeIndex child)
{
	parent->delChild(child);
	child->delParent(parent);
	edges--;
}
void DTNodeManager::deleteNodes(NodeIndex node)
{
	if(node == nullptr || node->Type() <= DT_LIT) return;
	assert(node->Parents().size() == 0);

	std::stack<NodeIndex> stk;
	stk.push(node);

	while(!stk.empty()) {
		NodeIndex v = stk.top();
		stk.pop();

		for(auto c : v->Children()) {
			c->delParent(v); edges--;
			if(c->Parents().empty() && c->Type() != DT_LIT)
				stk.push(c);
		}
		v->Children().clear();

		removeNode(v);
	}
}
NodeIndex DTNodeManager::getDCNode(Glucose1::Var x)
{
	NodeIndex dc = DontCare(x);
	if(Node(dc).Children().size()==0) {
		Node(dc).reset(DT_DC, x);
		addEdge(dc, Literal(x, false));
		addEdge(dc, Literal(x, true));
		nodes++;
	}
	assert(Node(DontCare(x)).Children().size()==2);
	return dc;
}
NodeIndex DTNodeManager::OR(Glucose1::Var x, NodeIndex first, NodeIndex second)
{
    if(isBottom(first))
        return second;
    else if(isBottom(second))
        return first;
    else if(first->Type() == DT_LIT && second->Type() == DT_LIT)
        return getDCNode(x);
    else {
    	NodeIndex v = newNode(DT_OR, x+1);
    	addEdge(v, first);
    	addEdge(v, second);
    	return v;
    }
}
void DTNodeManager::compressANDs()
{
	NodeIndex r = Root();

	char checked = ++state;

	stack<NodeIndex> target;
	stack<NodeIndex> search;

	if(!isBottom(r) && !isTop(r)) {
		search.push(r);
		r->setState(checked);
	}

	while(!search.empty()) {
		NodeIndex v = search.top();
		search.pop();

		if(v->Type() == DT_OR) {
			for(auto c : v->Children()) {
				if(c->State() == checked) continue;
				if(c->Type() == DT_AND || c->Type() == DT_OR) {
					search.push(c);
					c->setState(checked);
				}
			}
		}
		else if(v->Type() == DT_AND) {
			for(auto c : v->Children()) {
				if(c->State() == checked) continue;
				if(c->Type() == DT_OR) {
					search.push(c);
				}
				else if(c->Type() == DT_AND) {
					if(c->Parents().size() == 1)
						target.push(c);
					else
						search.push(c);
				}
				c->setState(checked);
			}

			while(!target.empty()) {
				NodeIndex d = target.top();
				target.pop();

				delEdge(v, d);
				for(auto e : d->Children()){
					e->delParent(d); edges--;
					addEdge(v, e);
					if(e->State() == checked) continue;
					if(e->Type() == DT_OR) {
						search.push(e);
					}
					else if(e->Type() == DT_AND) {
						if(e->Parents().size() == 1)
							target.push(e);
						else
							search.push(e);
					}
					e->setState(checked);
				}
				d->Children().clear();
				removeNode(d);
			}
		}
	}
}
template <typename T_data>
T_data DTNodeManager::countModel(Glucose1::vec<T_data>& lit_weight, bool wc)
{
	NodeIndex r = Root();
	if(isTop(r)) return 1;
	if(isBottom(r)) return 0;

	vector<T_data> mc;
	mc.resize(nodes+2*vars, 0);

	char checked = ++state;
	unsigned int id = 0;

	for(int i=0; i<vars; i++) {
		for(Glucose1::Lit l : {Glucose1::mkLit(i), ~Glucose1::mkLit(i)})
			if(Literal(l)->Parents().size()>0 || Literal(l) == root) {
				mc[id] = wc ? lit_weight[toInt(l)] : 1;
				Literal(l)->setId(id++);
				Literal(l)->setState(checked);
			}
	}

	stack<std::pair<NodeIndex, vector<NodeIndex>::iterator>> stack;
	stack.push(std::make_pair(r, r->Children().begin()));

	while(!stack.empty()) {
		auto ent = stack.top();

		if(ent.first->State() == checked) {
			stack.pop();
			continue;
		}

		if(ent.second == ent.first->Children().end()) {
			if(ent.first->Type() == DT_AND) {
				mc[id] = 1;
				for(auto c : ent.first->Children()){
					mc[id] *= mc[c->Id()];
				}
			} else if(ent.first->Type() == DT_OR || ent.first->Type() == DT_DC) {
				assert(ent.first->Children().size()==2);
				for(auto c : ent.first->Children()){
					mc[id] += mc[c->Id()];
				}
			}

			ent.first->setId(id++);
			ent.first->setState(checked);
			stack.pop();
		}
		else {
			NodeIndex c = *(ent.second);
			stack.top().second++;
			stack.push(std::make_pair(c, c->Children().begin()));
		}
	}

	T_data result = mc[r->Id()];
	return result;
}
template mpz_class DTNodeManager::countModel(Glucose1::vec<mpz_class>& lit_weight, bool wc);

void DTNodeManager::printNode(NodeIndex node, ostream& out) const
{
	if(isBottom(node))
		out << "O 0 0" << endl;
	else if(isTop(node))
		out << "A 0" << endl;
	else {
		auto v = node;

		switch(v->Type()) {
		case DT_LIT:
			out << "L " << v->Literal() << endl; break;
		case DT_DC:
			out << "O " << v->DecisionVar() << " 2 "
			<< Literal(Glucose1::mkLit(v->DecisionVar()-1))->Id() << " "
			<< Literal(~Glucose1::mkLit(v->DecisionVar()-1))->Id() << endl; break;
		case DT_AND:
			out << "A " << v->Children().size();
			for(auto c : v->Children())
				out << " " << c->Id();
			out << endl;
			break;
		case DT_OR:
			assert(v->Children().size() == 2);
			out << "O " << v->DecisionVar() << " 2 " << v->Children()[0]->Id() << " " << v->Children()[1]->Id() << endl; break;
		default:
			assert(false);
		}
	}
}
void DTNodeManager::printNNF(ostream& out)
{
	NodeIndex r = Root();

	if(isTop(r)) {
		out << "nnf 1 0 " << vars << endl;
		printNode(Top(), out);
		return;
	}
	if(isBottom(r)) {
		out << "nnf 1 0 " << vars << endl;
		printNode(Bottom(), out);
		return;
	}

	unsigned int id = 0;
	for(int i=0; i<vars; i++) {
		for(NodeIndex j : {Literal(Glucose1::mkLit(i)), Literal(~Glucose1::mkLit(i))})
			if(j->Parents().size()>0 || j == root)
				j->setId(id++);
			else
				j->setId(UINT_MAX);
	}

	out << "nnf " << (nodes+id) << " " << edges << " " << vars << endl;

	char checked = ++state;

	for(int i=0; i<vars; i++) {
		for(NodeIndex j : {Literal(Glucose1::mkLit(i)), Literal(~Glucose1::mkLit(i))}) {
			if(j->Id() < UINT_MAX)
				printNode(j, out);
			j->setState(checked);
		}
	}

	stack<std::pair<NodeIndex, vector<NodeIndex>::iterator>> stack;
	stack.push(std::make_pair(r, r->Children().begin()));

	while(!stack.empty()) {
		auto ent = stack.top();

		if(ent.first->State() == checked) {
			stack.pop();
			continue;
		}

		if(ent.second == ent.first->Children().end()) {
			ent.first->setId(id++);
			printNode(ent.first, out);
			ent.first->setState(checked);
			stack.pop();
		}
		else {
			NodeIndex c = *(ent.second);
			stack.top().second++;
			stack.push(std::make_pair(c, c->Children().begin()));
		}
	}
}
void DTNodeManager::printStats() const
{
	cout << "c o " << nodes << " internal nodes, " << edges << " edges, " << vars << " vars" << endl;
}


