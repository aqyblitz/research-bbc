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

// Local function declarations
static void SendCmd (int clientId, const CmdVector&  c_vector);
static void SendData(int clientId, const DataVector& d_vector);
static void print_adj_matrix(vector<int32_t>& matrix, int n);

void main_loop();

// Global variables
server_t           server;
client_t           client;
fd_set             fdr;
fd_set             fdw;

int                state; // // ** TODO: STATE DATA, move into struct. Encodes state
int                conn_count; // ** TODO: STATE DATA, move into struct. Counter for connections
int                k; // ** TODO: STATE DATA, move into struct. 			Current k
int                ack_count; // ** TODO: STATE DATA, move into struct. 	Counter for acknowledge messages

int        	       block_total; // # of distinct blocks
int        	       red_mult; // TODO: redundancy multiplier
int	             vertex_total; // # of vertices in graph
int                block_size; // # of vertices represented per block

bool               req_block; // ** TOD;O: STATE DATA, move into struct

vector<int32_t>    adj_matrix;
vector<int32_t>    row_k;
vector<int>        clients;
vector<int32_t>    solution;
CmdVector          cmd_vector;
DataVector         data_vector;
AckMsg             ack_curr;

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

    if(state != 0) {
        cout << "connection limit reached: no longer accepting connections" << endl;
        return;
    }

    // Add the new client to the client list
    if (clientId)
    {
        FD_SET(clientId, &fdr);
        FD_SET(clientId, &fdw);
        
        int send_size;
        int offset;
        data_vector.id=conn_count;
    
        if(conn_count == block_total && vertex_total%block_size > 0) // Edge case for last node with rows != vertex_total/block_total
        {
            send_size = vertex_total*(vertex_total-(block_total-1)*block_size); //*sizeof(int32_t);
            offset = (conn_count-1)*vertex_total*(int)ceil((1.0*vertex_total)/block_total);
        }
        else // standard case
        {
            send_size = vertex_total*(block_size);//*sizeof(int32_t);
            offset = (conn_count-1)*vertex_total*block_size;
        }

        data_vector.data.assign(&adj_matrix[0]+offset,&adj_matrix[0]+offset+send_size);
        SendData(clientId, data_vector);
        clients.push_back(clientId); // TODO: not fault-tolerant

        conn_count++;
        if(conn_count == red_mult*block_total) {
            state=1;
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
    if(state == 1 && req_block)
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

            row_k=data_vector.data; // TODO: How safe is this?
            state=2;
            req_block=false;
        }
    }
    if(state==3 && ack_count<block_total*red_mult)
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
                ack_count++;
            }
        }        
    }
    if(state == 4 && req_block && k<vertex_total)
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
            k++;
            req_block=false;
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

    printf("clientId: %d closed\n", clientId);
}

//////////////////////////////////////////////////////////////////////////////
//
// main_loop
//
///////////////////////////////////////////////////////////////////////////////
void main_loop()
{
    if(state==0) // still waiting on connections
        return;
    if(state==1 && !req_block) // request row
    {
        req_block=true;
        cmd_vector.c=0; // request code
        cmd_vector.k=k;
        
        SendCmd(clients[k/(block_size)],cmd_vector);
        
    }
    if(state==2) // send row
    {   
        for(int j=0;j<block_total*red_mult;j++)
        {
            cmd_vector.c=1;
            // k remains at the value set in state 1
            cmd_vector.data=row_k;
            SendCmd(clients[j], cmd_vector);
        }
        state=3;
    }
    if(state==3 && ack_count==block_total*red_mult) // completed counting acknolwedge msgs
    {
        ack_count=0;
 
        if(k==vertex_total-1)
        {
            state=4; // finish up
            k=0;
        }
        else
        {
            state=1; // continue to iterate
            k++; // req_block should already be reset to false here
        }
    }
    if(state==4 && !req_block && k<vertex_total)
    {
        req_block=true;
        cmd_vector.c=0; // request code
        cmd_vector.k=k;
        
        SendCmd(clients[k/(block_size)],cmd_vector);
    }
    
}

//////////////////////////////////////////////////////////////////////////////
//
// SendData
//
///////////////////////////////////////////////////////////////////////////////
static void SendData(int clientId, const DataVector& d_vector)
{
    cout << "SendData called" << endl;

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
        archive(d_vector);

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
    state=0;
    conn_count=0;
    k=0;
    ack_count=0;
    req_block=false;
    
    block_total=2;
    red_mult=1;
    vertex_total=4;
    block_size=vertex_total/block_total;

    data_vector.v_t=vertex_total;
    data_vector.b_t=block_total;
    adj_matrix.assign(hard, hard+sizeof(hard)/sizeof(int32_t));
    //row_k.reserve(vertex_total);
    //solution.reserve(vertex_total*vertex_total);

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
    addr.sin_port = htons(8888);

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

    emscripten_set_main_loop(main_loop, 60, 1);

    return EXIT_SUCCESS;
}
