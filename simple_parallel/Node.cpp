#include "Node.h"

class Node {

	Node::Node(int node_id, int n, int* adj_temp_sublist) {
		id = node_id;
		v_count = n;
		adj_sublist = adj_temp_sublist;
		bcast_node = null;
		ack = 0;
		filtered = false;
		rows = adj_temp_sublist.size()/n;
		end = false;

		start_threads();
	}

	Node::~Node() {
	}

	void Node::start_threads();
		std::thread receive_complete (receive_complete_signal);
		std::thread receive (recieve_thread_impl);
		std::thread broadcast (broadcast_row_thread);

		while(true) {
			if(end==true) {
				receive_complete.terminate();
				receive.terminate();
				broadcast.terminate();
				break;
			}
		}
	}

	void Node::end_threads() {
		end = true;
	}

	void Node::receive_complete_signal() {
		while(true) {
			if(this == bcast_node) {
				if(ack == v_count-1) {
					cast_row = null;
					bcast_node = null;
					ack = 0;

					if(pos == v_count-1) {
						end_threads();
					}
				}
			}
		}
	}

	void Node::receive_thread_impl() {
		while(true) {
			while(cast_row) {
				this.relax();
				boost::mutex::scoped_lock lock(ack_mutex);
				ack++;
				boost::mutex::scoped_lock unlock(ack_mutex);
				while(ack != 0) {}
				
			}
		}
	}

	void Node::set_pos(int p) {
		p = pos;
	}

	void Node::broadcast_row_thread() {
		while(true) {
			while(pos != -1 && !filtered) {
				if(pos/rows == id) {
					int temp[v_count];
					for(int i=0; i<v_count; i++)
						temp[i] = adj_sublist[pos-(pos/rows)*rows];
					cast_row = temp;
					pos = -1;
					filtered = false;
					bcast_node = this;
					continue;
				}
				else
					filtered = true;
		}
	}

	void Node::relax () {
		for(int i=0; i<rows; i++)
 			for(int j=0; j<v_count; j++)
				for(int k=0; k<d; k++) {
					int comp = adj_sublist[i*d+k] + cast_row[j];
					if(adj_sublist[i*d+j] > comp && comp >= 0) {
						adj_sublist[i*d+j] = comp;
					}
				}

		return;
	}

	int* Node::get_array() {
		return adj_sublist;
	}

}
