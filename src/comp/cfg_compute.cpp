/*
 * cfg_compute.cpp
 *
 *  Created on: May 22, 2019
 *      Author: zqzuo
 */


#include "cfg_compute.h"
#include "edgesToDelete.h"

void CFGCompute::do_worklist(CFG* cfg, GraphStore* graphstore){
	Concurrent_Worklist<CFGNode*>* worklist_1 = new Concurrent_Worklist<CFGNode*>();

	//initiate concurrent worklist
	std::vector<CFGNode*> nodes = cfg->getNodes();
	for(auto it = nodes.cbegin(); it != nodes.cend(); ++it){
		worklist_1->push(*it);
	}

	Concurrent_Worklist<CFGNode*>* worklist_2 = new Concurrent_Worklist<CFGNode*>();
	while(!worklist_1->isEmpty()){
		std::vector<std::thread> comp_threads;
		for (unsigned int i = 0; i < num_threads; i++)
			comp_threads.push_back(std::thread( [=] {this->compute(cfg, graphstore, worklist_1, worklist_2);}));

		for (auto &t : comp_threads)
			t.join();

		assert(worklist_1->isEmpty());
		Concurrent_Worklist<CFGNode*>* worklist_tmp = worklist_1;
		worklist_1 = worklist_2;
		worklist_2 = worklist_tmp;
		assert(worklist_2->isEmpty());
	}

	//clean
	delete(worklist_1);
	delete(worklist_2);

}


void CFGCompute::compute(CFG* cfg, GraphStore* graphstore, Concurrent_Worklist<CFGNode*>* worklist_1, Concurrent_Worklist<CFGNode*>* worklist_2){
	Grammar *grammar = new Grammar();
	/* TODO: load grammar from file 
	 * grammar->loadGrammar(filename);
	 */

	CFGNode* cfg_node;
	while(worklist_1->pop_atomic(cfg_node)){
		//merge
		PEGraph* in = combine(cfg, graphstore, cfg_node);

		//transfer
		PEGraph* out = transfer(in, cfg_node->getStmt(),grammar);

		//clean in
		delete in;

		//update and propagate
		PEGraph_Pointer out_pointer = cfg_node->getOutPointer();
		PEGraph* old_out = graphstore->retrieve(out_pointer);
		if(!out->equals(old_out)){
			//update out
			graphstore->update(out_pointer, out);

			//propagate
			std::vector<CFGNode*> successors = cfg->getSuccessors(cfg_node);
			for(auto it = successors.cbegin(); it != successors.cend(); ++it){
				worklist_2->push_atomic(*it);
			}
		}

		//clean out
		delete out;
	}
}



PEGraph* CFGCompute::combine(const CFG* cfg, const GraphStore* graphstore, const CFGNode* node){
	//get the predecessors of node
	std::vector<CFGNode*> preds = cfg->getPredesessors(node);

	if(preds.size() == 0){//entry node
		//return an empty graph
		return new PEGraph();
	}
	else if(preds.size() == 1){
		CFGNode* pred = preds[0];
		PEGraph_Pointer out_pointer = pred->getOutPointer();
		PEGraph* out_graph = graphstore->retrieve(out_pointer);
		return out_graph;
	}
	else{

		for(auto it = preds.cbegin(); it != preds.cend(); it++){
			CFGNode* pred = *it;
			PEGraph_Pointer out_pointer = pred->getOutPointer();
			PEGraph* out_graph = graphstore->retrieve(out_pointer);
		}

	}


}

PEGraph* CFGCompute::transfer_copy(PEGraph* in, Stmt* stmt,Grammar *grammar){
	PEGraph* out = new PEGraph(in);	

	// the KILL set
	std::set<vertexid_t> vertices;
	strong_update(stmt->getSrc(),out,vertices,grammar);
	
	// the GEN set
	peg_compute(out,stmt,grammar);

	return out;
}

PEGraph* CFGCompute::transfer_load(PEGraph* in, Stmt* stmt,Grammar *grammar){
	PEGraph* out = new PEGraph(in);	
	
	// the KILL set
	std::set<vertexid_t> vertices;
	strong_update(stmt->getSrc(),out,vertices,grammar);
	
	// the GEN set
	peg_compute(out,stmt,grammar);

	return out;
}

PEGraph* CFGCompute::transfer_store(PEGraph* in, Stmt* stmt,Grammar *grammar){
	PEGraph* out = new PEGraph(in);	
	// the KILL set
	std::set<vertexid_t> vertices;
	 
	if(is_strong_update(stmt->getSrc(),out,grammar)) {
		strong_update(stmt->getSrc(),out,vertices,grammar);
	}
	// the GEN set
	peg_compute(out,stmt,grammar);

	return out;
}

PEGraph* CFGCompute::transfer_address(PEGraph* in, Stmt* stmt,Grammar *grammar){
	PEGraph* out = new PEGraph(in);	
	
	// the KILL set
	std::set<vertexid_t> vertices;
	strong_update(stmt->getSrc(),out,vertices,grammar);
	
	// the GEN set
	peg_compute(out,stmt,grammar);
	
	return out;
}

bool CFGCompute::is_strong_update(vertexid_t x,PEGraph *out,Grammar *grammar) {
	/* If there exists one and only one variable o,which 
	 * refers to a singleton memory location,such that x and o are memory alias
	 */
	int numOfSingleTon = 0;
	int numEdges = out->getNumEdges(x - out->getFirstVid());
	vertexid_t* edges = out->getEdges(x - out->getFirstVid());
	label_t *labels = out->getLabels(x - out->getFirstVid());

	for(int i = 0;i < numEdges;++i) {
		if(grammar->isMemory(labels[i]) && out->isSingleton(edges[i]))
			++numOfSingleTon;	
	}
	return (numOfSingleTon == 1);
}

void CFGCompute::strong_update(vertexid_t x,PEGraph *out,std::set<vertexid_t> &vertices,Grammar *grammar) {
	// vertices <- must_alias(x)
	must_alias(x,out,vertices,grammar);

	/* foreach v in vertices do
	 * delete all the direct incoming and outgoing a edges of v from OUT
	 * delete all the incoming and outgoing M or V edges of v induced previously by the deleted a edges from OUT
	 */
	vertexid_t numVertices = out->getNumVertices();
	EdgesToDelete *deleteSet = new EdgesToDelete[numVertices];
	for(vertexid_t i = 0;i < numVertices;++i) {
		deleteSet[i] = EdgesToDelete();	
	}

	for(vertexid_t i = 0;i < numVertices;++i) {
		int numEdges = out->getNumEdges(i);
		vertexid_t *edges = out->getEdges(i);
		label_t *labels = out->getLabels(i);
		for(int j = 0;j < numEdges;++j) {
			if(isDirectAssignEdges(i + out->getFirstVid(),edges[j],labels[j],vertices,grammar)) {
					
				// delete the direct incoming and outgoing assign edges
				deleteSet[i].addOneEdge(edges[j],labels[j]);
				
				// delete s-rule edges induced previously by the deleted assign edges
				label_t s_rule_label = grammar->checkRules(labels[j]);	
				if(s_rule_label != (char)127)
					deleteSet[i].addOneEdge(edges[j],s_rule_label);	
				
				// delete d-rule 
				vertexid_t *_edges = out->getEdges(edges[j] - out->getFirstVid());
				label_t *_labels = out->getLabels(edges[j] - out->getFirstVid());
				int _numEdges = out->getNumEdges(edges[j] - out->getFirstVid());
				for(int k = 0;k < _numEdges;++k) {
					label_t d_rule_label = grammar->checkRules(labels[j],_labels[k]);
					if(d_rule_label != (char)127)
						deleteSet[i].addOneEdge(_edges[k],d_rule_label);		
				}		
			}
			else {
				vertexid_t *_edges = out->getEdges(edges[j] - out->getFirstVid());
				label_t *_labels = out->getLabels(edges[j] - out->getFirstVid());
				int _numEdges = out->getNumEdges(edges[j] - out->getFirstVid());
				for(int k = 0;k < _numEdges;++k) {
					
					// only delete d-rule edges, because assign edges and s-rule have been deleted already.		 
					if(isDirectAssignEdges(edges[j],_edges[k],_labels[k],vertices,grammar)) {
						label_t d_rule_label = grammar->checkRules(labels[j],_labels[k]);
						if(d_rule_label != (char)127) 
							deleteSet[i].addOneEdge(_edges[k],d_rule_label);	
					}
				}
			}		
		}
	}

	/* merge and remove duplicate edges
	 * update OUT,delete edges in OUT
	 * clean
	 */
	for(vertexid_t i = 0;i < numVertices;++i) {
		deleteSet[i].merge();	
		int len = 0; int n1 = out->getNumEdges(i); int n2 = deleteSet[i].getRealNumEdges();
		vertexid_t *edges = new vertexid_t[n1];
		label_t *labels = new label_t[n1];
		myalgo::minusTwoArray(len,edges,labels,n1,out->getEdges(i),out->getLabels(i),n2,deleteSet[i].getEdges(),deleteSet[i].getLabels());
		
		if(len) 
			out->setEdgeArray(i,len,edges,labels);
		else
			out->clearEdgeArray(i);
			
		delete[] edges; delete[] labels;
		deleteSet[i].clear();
	}
	delete[] deleteSet;
}

bool CFGCompute::isDirectAssignEdges(vertexid_t src,vertexid_t dst,label_t label,std::set<vertexid_t> &vertices,Grammar *grammar) {
	if(!grammar->isAssign(label))
		return false;
	std::set<vertexid_t>::iterator iter;
	return ( ((iter = vertices.find(src)) != vertices.end()) || ((iter = vertices.find(dst)) != vertices.end()) );
}

void CFGCompute::must_alias(vertexid_t x,PEGraph *out,std::set<vertexid_t> &vertices,Grammar *grammar) {
	/* if there exists one and only one variable o,which
	 * refers to a singleton memory location,such that x and
	 * y are both memory aliases of o,then x and y are Must-alias
	 */
	vertexid_t o;	
	int numOfSingleTon = 0;
	int numEdges = out->getNumEdges(x - out->getFirstVid());
	vertexid_t *edges = out->getEdges(x -out->getFirstVid());
	label_t *labels = out->getLabels(x - out->getFirstVid());
	for(int i = 0;i < numEdges;++i) {
		if(grammar->isMemory(labels[i]) && out->isSingleton(edges[i])) {
			++numOfSingleTon;
			o = edges[i];
		}
	}
	if(numOfSingleTon != 1) 
		return;

	numEdges = out->getNumEdges(o - out->getFirstVid());
	edges = out->getEdges(o - out->getFirstVid());
	labels = out->getLabels(o - out->getFirstVid());
	for(int i = 0;i < numEdges;++i) {
		if(grammar->isMemory(labels[i]) && (edges[i] != x) && is_strong_update(edges[i],out,grammar))
			vertices.insert(edges[i]);	
	}		
}

void CFGCompute::peg_compute(PEGraph *out,Stmt *stmt,Grammar *grammar) {
	
	// add assgin edge based on stmt, (out,assign edge) -> compset 	
	ComputationSet *compset = new ComputationSet();
	initComputationSet(*compset,out,stmt);

	// start GEN
	PEGCompute pegCompute;
	pegCompute.startCompute(*compset,grammar);

	// GEN fininshed, compset -> out
	vertexid_t numVertices = out->getNumVertices();
	for(vertexid_t i = 0;i < numVertices;++i) {
		if(compset->getOldsNumEdges(i))
			out->setEdgeArray(i,compset->getOldsNumEdges(i),compset->getOldsEdges(i),compset->getOldsLabels(i));			
		else 
			out->clearEdgeArray(i);		
	}
	// clean
	compset->clear();
	delete compset;	
}

void CFGCompute::initComputationSet(ComputationSet &compset,PEGraph *out,Stmt *stmt) {
	compset.init(out,stmt);	
}
