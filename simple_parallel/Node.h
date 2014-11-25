#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <cstdlib>

class Node {
	static int pos;
	static int* cast_row;
	static Node* bcast_node;
	static std::mutex ack_mutex;
	static int ack;
	static bool end;
	public:
		Node(int id, int n, int* adj_temp_sublist);
		~Node();
		void set_pos();
		void end_threads();
		int* get_array();
	private:
		void start_threads();
		void relax();
		void receive_complete_signal();
		void receive_thread_impl();
		void broadcast_row_thread();

		int id;
		int rows;
		bool filtered;
		int* adj_sublist;
		int v_count;
};

#endif
