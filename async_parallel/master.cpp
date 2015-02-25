// All the necessary imports here.

// TODO
// - we're sending the data back to all the sockets for relaxation...including the one we got it from. Nah.

int main() {
	fd_set readfds;
	setup();

	int32_t state=0;
	int32_t slave_count=0;
	int32_t k=0;
	bool requested=false;
	int32_t ack=0;
	int32_t ack_c=0;
	int32_t max_clients; // pre-determined
	int32_t vertex_total; // pre-determined
	int slave_total; // pre-determined
	while(true)
	{
		emscriptem_main(state,k,slave_count,slave_total,requested,ack);
	}

	teardown();

	return 0;
}

void setup() {
	// Setup socket stuff.
	// Initialize arrays.
}

void teardown() {
	// Close connections.
	// Delete stuff allocated with new.
}

void build_fd_table() {
	// Build the fd table. To be called after EVERY select.
}

void emscriptem_main(fd_set& readfds, int32_t &s, int32_t& k, int32_t& ack_c, int32_t& slave_count, const int32_t& slave_total, const int32_t& vertex_total, const int32_t& max_clients, bool& requested, int32_t& ack) {
	if(s==0) // state 0
	{
		build_fd_table();
		select(); // poll sockets
		if(FD_ISSET(&readfds)) // somethin happnd!
		{
			// add connection
			// send data [info]
			// send data [adj block]
			slave_count++;
		}
		if(slave_count == slave_total) // [RED]
		{
			s++;
		}
	}
	
	if(s==1)
	{
		if(k==vertex_total)
		{
			s=4;
			requested=true; // so the rest of this won't trigger	
		}

		if(!requested)
		{
			// request row k
			// [READ] row_k
			if(read_completed)
			{
				requested=true;
				// send data to all slaves
				s++;
			}
	}

	if(s==2)
	{
		activity=select(); // poll sockets
		if(activity>0)
		{
			s=3;
		}
		else if(ack==slave_total)
		{
			k++;
			ack=0;
			requested=false;
			s=1;
		}
	}

	if(s==3)
	{
		sd = client_socket[ack_c];
		if(FD_ISSET(sd,readfds))
		{

		}
		// [READ] ack message (the id) which is and [TODO] will encode the unique block of the slave
		if(read_completed)
		{
			ack++;
		}	
	}
}

