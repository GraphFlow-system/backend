/*
 * cfg_map.h
 *
 *  Created on: Jun 11, 2019
 *      Author: zqzuo
 */

#ifndef COMP_CFG_MAP_H_
#define COMP_CFG_MAP_H_

#include "cfg.h"
using namespace std;

class CFG_map : public CFG {

//	friend std::ostream & operator<<(std::ostream & strm, const CFG_map& cfg){
//		cfg.print(strm);
//		return strm;
//	}

public:
    CFG_map(){}

//    CFG_map(std::string file){
//		std::ifstream fin;
//		fin.open(file);
//		if (!fin) {
//			cout << "can't load file_cfg: " << file << endl;
//		}
//    }

    CFG_map(const string& file_cfg, const string& file_stmt){

    }

    ~CFG_map(){
        for (auto &node : nodes) {
            delete node;
        }
    }


    void loadCFG(const string& file_cfg, const string& file_stmt) {
		// handle the stmt file
		std::ifstream fin;
		fin.open(file_stmt);
		if (!fin) {
			cout << "can't load file_stmt " << endl;
			exit(EXIT_FAILURE);
		}

		std::map<int, CFGNode*> m;

		std::string line;
		while (getline(fin, line) && line != "") {
			std::cout << line << "\n";

			std::stringstream stream(line);
			std::string stmt_id, type, dst, src, added;
			stream >> stmt_id >> type >> dst >> src >> added;

			std::cout << stmt_id << "," << type << "," << dst << "," << src
					<< "," << added << "\n";

			TYPE t;
			if (type == "assign") {
				t = TYPE::Assign;
			}
			if (type == "load") {
				t = TYPE::Load;
			}
			if (type == "store") {
				t = TYPE::Store;
			}
			if (type == "alloca") {
				t = TYPE::Alloca;
			}

			Stmt* stmt = new Stmt(t, atoi(src.c_str()), atoi(dst.c_str()),
					atoi(added.c_str()));
			CFGNode* cfgNode = new CFGNode(atoi(stmt_id.c_str()), stmt);
			m[atoi(stmt_id.c_str())] = cfgNode;

			//add cfgnode into cfg
			this->addOneNode(cfgNode);

			//add entry node
			if(cfgNode->getCfgNodeId() == 0){
				this->nodes_entry.push_back(cfgNode);
			}
		}
		fin.close();

		//handle the cfg.txt
		fin.open(file_cfg);
		if (!fin) {
			cout << "can't load file_cfg: " << file_cfg << endl;
			exit(EXIT_FAILURE);
		}

		while (getline(fin, line) && line != "") {
			std::stringstream stream(line);
			std::string pred_id, succ_id;
			stream >> pred_id >> succ_id;

			//        cfgMap->addOneNode(m[atoi(pred_id.c_str())]);
			//        cfgMap->addOneNode(m[atoi(succ_id.c_str())]);
			this->addOneSucc(m[atoi(pred_id.c_str())],
					m[atoi(succ_id.c_str())]);
			this->addOnePred(m[atoi(succ_id.c_str())],
					m[atoi(pred_id.c_str())]);
		}
		fin.close();
    }

//    void loadCFG(const string& file_cfg, const string& file_stmt, const string& file_mirrors_in, const string& file_mirrors_out) {
//    	cout << "invalid function call!" << endl;
//    	exit(EXIT_FAILURE);
//    }


    std::vector<CFGNode*> getPredesessors(const CFGNode* node) const override{
        auto it = predes.find(node);
        if(it != predes.end()){
            return it->second;
        }
        else{
        	return std::vector<CFGNode*>();
//            perror("invalid key!");
//            exit (EXIT_FAILURE);
        }
    }

    std::vector<CFGNode*> getSuccessors(const CFGNode* node) const override {
        auto it = succes.find(node);
        if(it != succes.end()){
            return it->second;
        }
        else{
        	return std::vector<CFGNode*>();
//            perror("invalid key!");
//            exit (EXIT_FAILURE);
        }
    }

    std::vector<CFGNode*> getNodes() const override {
        return nodes;
    }

    std::vector<CFGNode*> getEntryNodes() const override {
    	return nodes_entry;
    }


    void addOneNode(CFGNode *Node)  {
        nodes.push_back(Node);
    }


    void addOnePred(CFGNode *succ, CFGNode *pred)  {
		if(predes.find(succ) == predes.end()) {
			predes[succ] = std::vector<CFGNode*>();
		}
		predes[succ].push_back(pred);
    }

    void addOneSucc(CFGNode *pred, CFGNode *succ)  {
    	if(succes.find(pred) == succes.end()){
    		succes[pred] = std::vector<CFGNode*>();
    	}
		succes[pred].push_back(succ);
		this->number_edges++;
    }

    vertexid_t getNumberEdges() const {
    	return number_edges;
    }

    void print(std::ostream& str) const override {
    	str << "The number of nodes in CFG: \t" << this->nodes.size() << "\n";
    	str << "The number of edges in CFG: \t" << this->getNumberEdges() << "\n";
    	for(auto it = this->succes.begin(); it != this->succes.end(); ++it){
    		const CFGNode* node = it->first;
    		for(auto& n: it->second){
    			str << *node << "\t" << *n << "\n";
    		}
    	}
    }


protected:

    std::vector<CFGNode*> nodes;

    std::vector<CFGNode*> nodes_entry;

    vertexid_t number_edges = 0;

    std::unordered_map<const CFGNode*, std::vector<CFGNode*>> predes;

    std::unordered_map<const CFGNode*, std::vector<CFGNode*>> succes;



};



#endif /* COMP_CFG_MAP_H_ */
