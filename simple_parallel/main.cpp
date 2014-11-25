#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>

#include "Node.h"

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);
void do_floyd_warshall(int dim, int* a_flat);

int main(int argc, char* argv[]) {
	if(argc<2) {
		std::cout << "Usage: " << argv[0] << "data location" << std::endl;
	}

	// Things to initialize to parse the file.	
	std::string line;
	std::ifstream data_file (argv[1]);

	// Just to count the number of lines.
	std::ifstream data_file_count (argv[1]);
	int v_count = std::count(std::istreambuf_iterator<char>(data_file_count), std::istreambuf_iterator<char>(), '\n');
	data_file_count.close();

	int i=0;
	std::vector<std::string> file_by_line[v_count];
	std::string names[v_count];
        std::vector<std::string> temp;

	// Store names, load up the split vectors.
	if(data_file.is_open()) {
		while(getline(data_file,line)) {
			//std::cout << line << '\n';

			temp = split(line,' ');
			names[i] = temp[0];
			temp.erase(temp.begin());
			//std::vector<std::string> sub_vec(temp.begin()+1, sub_vec.end());
                        //file_by_line[i] = &sub_vec;
			file_by_line[i] = temp;
			//std::cout << names[i] << '\n';
			i++;
		}
		data_file.close();
	}

	// Finally, build the adjacency matrix.
	int adj_matrix[v_count*v_count];
	for(int i=0; i<v_count; i++) {
		for(int j=0; j<v_count; j++) {
			adj_matrix[i*v_count+j] = atoi( file_by_line[i][j].c_str() );
                        std::cout << adj_matrix[i*v_count+j] << " ";
		}
		std::cout << std::endl;
	}

	std::cout << "--------------------" << std::endl;

	int dist[v_count*v_count];
	for(int i=0; i<v_count; i++) {
		for(int j=0; j<v_count; j++) {
			if(adj_matrix[i*v_count+j] == 0 && i!=j) {
				dist[i*v_count+j] = std::numeric_limits<int>::max();
			}
			else {
				dist[i*v_count+j] = adj_matrix[i*v_count+j];
			}
		}
	}

	//-----------------------
	// 1. Create nodes, separate lists.
	// 2. Initialize and connect signals.
	// 3. For loop through the list. Acquire row (call to all Nodes) --> broadcast set, broadcast to all other nodes --> other nodes call +1 to broadcast on node_cont --> node_cont finally returns after getting the signals
	int n_nodes = 3;
	Node* node_list[n_nodes];
	for(int i=0; i<n_nodes; i++) {
		int* temp[v_count/n_nodes*v_count];
		for(int j=0; j<v_count/n_nodes; j++) { //rows
			for(int k=0; k<v_count; k++) { //columns
				// rows per group * group id = how many rows to skip, * v_count = how many indices to skip
				temp[j*v_count+k] = dist[v_count/n_nodes*i*v_count+j*v_count+k];
			}
		}
		node_list[i] = new Node(i,v_count,temp);
	}

	for(int i=0; i<n_nodes; i++) {
		int* p_temp = node_list[i].get_array();
		for(int j=0; j<v_count/n_nodes; j++) {
			for(int k=0; k<v_count; k++) {
				std::cout << p_temp[j*v_count+k] << " ";
			}
			std::cout << std::endl;
		}
	}

	return 0;
}

// Found these split methods on Stack Overflow. Nice.
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}
