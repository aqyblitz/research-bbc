// All the necessary imports here.
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/socket.h>


using namespace std;

// TODO
// - we're sending the data back to all the sockets for relaxation...including the one we got it from. Nah.

void setup();
void teardown();
void emscriptem_main(fd_set& readfds, int32_t &s, int32_t& k, int32_t& ack_slave_c, int32_t& slave_count, const int32_t& slave_total, const int32_t& vertex_total, const int32_t& max_clients, bool& requested, int32_t& ack, bool& loop);

int main() {
	fd_set readfds;
	setup();

	bool loop=true;
	int32_t state=0;
	int32_t slave_count=0;
	int32_t k=0;
	bool requested=false;
	int32_t ack=0;
	int32_t ack_slave_c=0;
	int32_t max_clients; // pre-determined
	int32_t vertex_total; // pre-determined
	int slave_total; // pre-determined
	while(loop)
	{
		emscriptem_main(readfds,state,k,ack_slave_c,slave_count,slave_total,vertex_total,max_clients,requested,ack,loop);
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

void emscriptem_main(fd_set& readfds, int32_t &s, int32_t& k, int32_t& ack_slave_c, int32_t& slave_count, const int32_t& slave_total, const int32_t& vertex_total, const int32_t& max_clients, bool& requested, int32_t& ack, bool& loop) {
	int n, sd, activity;

	if(s==0) // state 0
	{
		build_fd_table();
		//activity=select(); // poll sockets
		if(activity>0)
		{
			for(;;)// all connections
			{
				//new_socket = accept();
				if(1) //FD_ISSET(master_socket, &readfds)) // somethin happnd!
				{
					// add connection
					// send data [info]
					// send data [adj block]
					slave_count++;
				}
				if(slave_count == slave_total) // [RED]
				{
					s=1;
					return;
				}
			}
		}
	}
	
	if(s==1)
	{
		if(k==vertex_total)
		{
			s=5;
			k=0; // going to re-use for retrieving answer
			requested=false;
			return;
		}

		if(!requested)
		{
			// request row k
			requested=true;
		}
		// n=read(&row_k)
		if(n==vertex_total*sizeof(int32_t)) // read completed
		{
			// send data to all slaves
			s=2;
		}
	}

	if(s==2)
	{
		build_fd_table();
		//activity=select(); // poll sockets
		if(activity>0)
		{
			s=3;
		}
	}

	if(s==3)
	{
		//sd = client_socket[ack_slave_c];
		if(FD_ISSET(sd,&readfds))
		{
			s=4;
		}
	}

	if(s==4)
	{
		// n=read(); // [READ] ack message (the id) which is and [TODO] will encode the unique block of the slave
		if(n==sizeof(int32_t))
		{
			ack++;
			ack_slave_c++; // NOT bundling this win ack for redundancy stuff in the future.
			if(ack==slave_total)
			{
				k++;
				ack=0;
				ack_slave_c=0;
				requested=false;
				s=1;
				return;
			}
		}	
	}

	if(s==5)
	{
      if(k==vertex_total)
      {
			loop=false;
			return;
      }
      if(!requested)
      {
         // request row k
		}
      // n=read(&row_k)
      if(n==vertex_total*sizeof(int32_t)) // read completed
      {
			// store data
			k++;
		}
   }
}	
