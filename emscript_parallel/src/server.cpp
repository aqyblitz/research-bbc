// System Includes
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <chrono> // Used for testing

#include <emscripten.h>

// Project Includes
#include "include/structs.h"

// TODO: Set config file to contain port and # of clients needed.

// Namespaces
using namespace std;

// Structs
typedef struct
{
  int fd;
  string url;
} server_t;

typedef struct
{
  int fd;
  struct sockaddr_in addr;
} client_t;

typedef struct
{
  int        	   block_total; // # of distinct blocks
  int        	   red_mult; // TODO: redundancy multiplier
  int	               vertex_total; // # of vertices in graph
  int                block_size; // # of vertices represented per block
  vector<int32_t>    adj_matrix; // adjacency matrix
} Constants;

typedef struct
{
  int  state; // Encodes state
  int  conn_count; // Counter for connections
  int  k; // Current k
  int  ack_count; // Counter for acknowledge messages
  bool req_block; // Bool to block until row_k received
} StateVars;  

// Local function declarations
static void SendCmd (int clientId, const CmdVector&  c_vector);
static void SendInit(int clientId, const InitVector& i_vector);
static void print_adj_matrix(vector<int32_t>& matrix, int n);

void main_loop();

// Global variables
server_t           server;
client_t           client;
Constants          c;
StateVars          s;
fd_set             fdr;
fd_set             fdw;

//vector<int32_t>    row_k; ** CUT-A
vector<int>        clients;
vector<int32_t>    solution;

InitVector         init_vector; // For sending initialization data to a node.
CmdVector          cmd_vector;  // For sending a command 

DataVector         data_vector; // For reading in solution data.
AckMsg             ack_curr;    // For reading a command

///////////////////////////////////////////////////////////////////////////////
//
// cleanup
//
///////////////////////////////////////////////////////////////////////////////
void cleanup()
{
   cout << "cleanup() called" << endl;

   //TODO: Update Cleanup
   if(client.fd)
   {
      close(client.fd);
      client.fd = 0;
   }

   if(server.fd)
   {
      close(server.fd);
      server.fd = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// cleanup
//
///////////////////////////////////////////////////////////////////////////////
void cleanup(int param)
{
   cout << "cleanup(int param) called" << endl;

   cleanup();
}

///////////////////////////////////////////////////////////////////////////////
//
// async_connection
//
///////////////////////////////////////////////////////////////////////////////
void async_connection(int clientId, void* userData)
{
    cout << "async_connection called by clientId " << clientId << endl;

    if(s.state != 0) {
        cout << "connection limit reached: no longer accepting connections" << endl;
        return;
    }

    // Add the new client to the client list
    if (clientId)
    {
        FD_SET(clientId, &fdr);
        FD_SET(clientId	, &fdw);
        
        int send_size;
        int offset;
        init_vector.id=s.conn_count; // THIS IS IMPORTANT TO NOTE: ID assigned in order of connection
    
        if(s.conn_count == c.block_total && c.vertex_total%c.block_size > 0) // Edge case for last node with rows != vertex_total/block_total
        {
            send_size = c.vertex_total*(c.vertex_total-(c.block_total-1)*c.block_size); //*sizeof(int32_t); is not necessary here b/c of how adding to a memory address works.
            offset = (s.conn_count-1)*c.vertex_total*(int)ceil((1.0*c.vertex_total)/c.block_total);
        }
        else // standard case
        {
            send_size = c.vertex_total*(c.block_size); // see above
            offset = (s.conn_count-1)*c.vertex_total*c.block_size;
        }

        init_vector.data.assign(&c.adj_matrix[0]+offset,&c.adj_matrix[0]+offset+send_size);
        SendInit(clientId, init_vector);
        clients.push_back(clientId); // TODO: not fault-tolerant. make this a map later if you can!

        s.conn_count++;
        if(s.conn_count == c.red_mult*c.block_total) {
            s.state=1;
            cout << "changing states" << endl;
        }
        cout << "succesfully exchanged data with connecting client" << endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// async_message
//
///////////////////////////////////////////////////////////////////////////////
void async_message(int clientId, void* userData)
{
    cout << "async_message called by clientId " << clientId << endl;
    if(s.state == 1 && s.req_block)
    { // Receive requested row, then transition to next state and send data out.
        unsigned int messageSize;
        if (ioctl(server.fd, FIONREAD, &messageSize) != -1)
        {
            cout << "messageSize: " << messageSize << endl;

            // Get Message
            char* message = new char[messageSize];
            recv(server.fd, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(data_vector);
            }

            // Clean up dynamically allocated memory
            delete[] message;

            //row_k=data_vector.data; ** CUT-A            
            cmd_vector.data=data_vector.data; // ** ADD-A
            s.state=2;
            s.req_block=false;
        }
    }
    if(s.state==3 && s.ack_count<c.block_total*c.red_mult)
    {
        unsigned int messageSize;
        if (ioctl(server.fd, FIONREAD, &messageSize) == sizeof(AckMsg))
        {
            cout << "ackMessageSize: " << messageSize << endl;
            
            // Get Message
            char* message = new char[messageSize];
            recv(server.fd, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(ack_curr);
            }

            if(ack_curr.status == 1) // && block ID not acknowledged
            {
                s.ack_count++;
            }
        }        
    }
    if(s.state == 4 && s.req_block && s.k<c.vertex_total)
    {
        unsigned int messageSize;
        if (ioctl(server.fd, FIONREAD, &messageSize) != -1)
        {
            cout << "messageSize: " << messageSize << endl;

            // Get Message
            char* message = new char[messageSize];
            recv(server.fd, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(data_vector);
            }

            // Clean up dynamically allocated memory
            delete[] message;

            for(int i=0; i<messageSize/sizeof(int32_t); i++)
            {
                solution.push_back(data_vector.data[i]);
            }
            s.k++;
            s.req_block=false;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// async_close
//
///////////////////////////////////////////////////////////////////////////////
void async_close(int clientId, void* userData)
{
    cout << "async_close called" << endl;

    string fileName;

    // Remove the clients file descriptors
    FD_CLR(clientId, &fdr);
    FD_CLR(clientId, &fdw);
    vector<int>::iterator p = find(clients.begin(), clients.end(), clientId);
    if(p != clients.end())
    {
        clients[distance(clients.begin(),p)]=0;
    }

    printf("clientId: %d closed\n", clientId);
}

//////////////////////////////////////////////////////////////////////////////
//
// main_loop
//
///////////////////////////////////////////////////////////////////////////////
void main_loop()
{
    if(s.state==0) // still waiting on connections
        return;
    if(s.state==1 && !s.req_block) // request row
    {
        s.req_block=true;
        cmd_vector.c=0; // request code
        cmd_vector.k=s.k;
        
        SendCmd(clients[s.k/(c.block_size)],cmd_vector);
        
    }
    if(s.state==2) // send row
    {   
        for(int j=0;j<c.block_total*c.red_mult;j++)
        {
            cmd_vector.c=1;
            // k remains at the value set in state 1
            // cmd_vector.data=row_k; ** CUT-A
            SendCmd(clients[j], cmd_vector);
        }
        s.state=3;
    }
    if(s.state==3 && s.ack_count==c.block_total*c.red_mult) // completed counting acknolwedge msgs
    {
        s.ack_count=0;
 
        if(s.k==c.vertex_total-1)
        {
            s.state=4; // finish up
            s.k=0;
        }
        else
        {
            s.state=1; // continue to iterate
            s.k++; // s.req_block should already be reset to false here
        }
    }
    if(s.state==4 && !s.req_block && s.k<c.vertex_total)
    {
        s.req_block=true;
        cmd_vector.c=0; // request code
        cmd_vector.k=s.k;
        // we can leave data blank
        
        SendCmd(clients[s.k/(c.block_size)],cmd_vector);
    }
    
}

//////////////////////////////////////////////////////////////////////////////
//
// SendInit
//
///////////////////////////////////////////////////////////////////////////////
static void SendInit(int clientId, const InitVector& i_vector)
{
    cout << "SendInit called" << endl;

    int res;

    if (!FD_ISSET(clientId, &fdw))
    {
        cout << "clientId " << clientId << " not set for writing." << endl;
        return;
    }

    // Serialize the data
    try
    {
        std::stringstream ss;
        cereal::PortableBinaryOutputArchive archive(ss);
        archive(i_vector);

        // Copy the stringstream to a char*
        const std::string& tmp = ss.str();
        const char* message = tmp.c_str();

        // Send the message
        res = send(clientId, message, tmp.size(), 0);

        if (res == -1) {
            cout << "ERROR: Exception thrown sending data to clientId " << clientId << endl;
            return;
        }
        else if (res == 0)
        {
            // Client disconnected
            memset(&client, 0, sizeof(client_t));
            return;
        }

        cout << "Sent " << tmp.size() << " bytes to client " << clientId << endl;
    }
    catch (...)
    {
        cout << "ERROR: could not serialize the node initialization data" << endl;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// SendCmd
//
///////////////////////////////////////////////////////////////////////////////
static void SendCmd(int clientId, const CmdVector& c_vector)
{
    cout << "SendCmd called" << endl;

    int res;

    if (!FD_ISSET(clientId, &fdw))
    {
        cout << "clientId " << clientId << " not set for writing." << endl;
        return;
    }

    // Serialize the data
    try
    {
        std::stringstream ss;
        cereal::PortableBinaryOutputArchive archive(ss);
        archive(c_vector);

        // Copy the stringstream to a char*
        const std::string& tmp = ss.str();
        const char* message = tmp.c_str();

        // Send the message
        res = send(clientId, message, tmp.size(), 0);

        if (res == -1) {
            cout << "ERROR: Exception thrown sending data to clientId " << clientId << endl;
            return;
        }
        else if (res == 0)
        {
            // Client disconnected
            memset(&client, 0, sizeof(client_t));
            return;
        }

        cout << "Sent " << tmp.size() << " bytes to client " << clientId << endl;
    }
    catch (...)
    {
        cout << "ERROR: could not serialize the display data" << endl;
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// print_adj_matrix
//
///////////////////////////////////////////////////////////////////////////////
void print_adj_matrix(vector<int32_t>& matrix, int n)
{
    for(int j=0; j<matrix.size(); j++)
    {
        if(j!=0 && j%n==0)
            printf("\n");
        printf("%d ",matrix[j]);
    }
    printf("\n");
}

///////////////////////////////////////////////////////////////////////////////
//
// main
//
///////////////////////////////////////////////////////////////////////////////
int main()
{
    // Local variables
    struct sockaddr_in addr;
    int res;

    // Initialize
    cout << "Initializing..." << endl;
    int32_t hard[16] = {0,0,2,0, 4,0,3,0, 0,0,0,2, 0,1,0,0};
    s.state=0;
    s.conn_count=0;
    s.k=0;
    s.ack_count=0;
    s.req_block=false;
    
    c.block_total=2;
    c.red_mult=1;
    c.vertex_total=4;
    c.block_size=c.vertex_total/c.block_total;

    init_vector.v_t=c.vertex_total;
    init_vector.b_t=c.block_total;
    c.adj_matrix.assign(hard, hard+sizeof(hard)/sizeof(int32_t));
    //row_k.reserve(c.vertex_total);
    //solution.reserve(c.vertex_total*c.vertex_total);

    FD_ZERO(&fdr);
    FD_ZERO(&fdw);

    atexit(cleanup);
    signal(SIGTERM, cleanup);

    memset(&server, 0, sizeof(server_t));
    memset(&client, 0, sizeof(client_t));

    // Create the socket and set to non-blocking
    server.fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(server.fd == -1)
    {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    fcntl(server.fd, F_SETFL, O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888); // HARDCODED PORT=8888

    if(inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr) != 1)
    {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    res = ::bind(server.fd, (struct sockaddr *)&addr, sizeof(addr));

    if(res == -1)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    res = listen(server.fd, 50); // max connections, second param

    if(res == -1)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // The first parameter being passed is actually an arbitrary userData pointer
    emscripten_set_socket_connection_callback((void *) NULL, async_connection);
    emscripten_set_socket_message_callback((void *) NULL, async_message);
    emscripten_set_socket_close_callback((void *) NULL, async_close);

    emscripten_set_main_loop_arg(main_loop, (void *) NULL, 60, 1);

    return EXIT_SUCCESS;
}
