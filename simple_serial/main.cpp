#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

int main(int argc, char* argv[]) {
	if(argc<2) {
		std::cout << "Usage: " << argv[0] << "data location" << std::endl;
	}

	// Things to initialize to parse the file.	
	std::string line;
	std::ifstream data_file (argv[1]);

	// Just to count the number of lines.
	std::ifstream data_file_count (argv[1]);
	int node_count = std::count(std::istreambuf_iterator<char>(data_file_count), std::istreambuf_iterator<char>(), '\n');
	data_file_count.close();

	int i=0;
	std::vector<std::string> file_by_line[node_count];
	std::string names[node_count];
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
	int adj_matrix[node_count][node_count];
	for(int i=0; i<node_count; i++) {
		for(int j=0; j<node_count; j++) {
			adj_matrix[i][j] = atoi( file_by_line[i][j].c_str() );
                        std::cout << adj_matrix[i][j] << " ";
		}
		std::cout << std::endl;
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
