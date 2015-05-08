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
#include <sstream>
#include <fstream>

#include <chrono> // Used for testing
#include <emscripten.h>

// Project Includes
#include "include/structs.h"

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
    int        	     block_total; // # of distinct blocks
    int                red_mult; // TODO: redundancy multiplier
    int	           vertex_total; // # of vertices in graph
    int                block_size; // # of vertices represented per block
    vector<int32_t>    adj_matrix; // adjacency matrix
} Constants;

typedef struct
{
    int                state; // Encodes state
    int                conn_count; // Counter for connections
    int                k; // Current k
    int                ack_count; // Counter for acknowledge messages
    bool               req_block; // Bool to block until row_k received
} StateVars;  

// Local function declarations
static void SendCmd (int clientId, const CmdVector&  c_vector);
static void SendInit(int clientId, const InitVector& i_vector);
static void print_solution();

void main_loop();

// Global variables
server_t           server;
client_t           client;
Constants          c;
StateVars          st;
fd_set             fdr;
fd_set             fdw;

bool               debug;
int                c_param;

vector<int32_t>    row_k;       // Stores the kth row.
vector<int>        clients;     // TODO: Make this the map above. List of all client fd's.
vector<int32_t>    solution;    // For taking the solution.

InitVector         init_vector; // For sending initialization data to a node.
CmdVector          cmd_vector;  // For sending a command 

DataVector         data_vector; // For reading in solution data.
AckMsg             ack_msg;    // For reading a command

///////////////////////////////////////////////////////////////////////////////
//
// cleanup
//
///////////////////////////////////////////////////////////////////////////////
void cleanup()
{
    if(debug)
        cout << "cleanup() called" << endl;

    //TODO: Update Cleanup
    for(int i=0; i<clients.size(); i++)
    {
        if(clients[i])
        {
            close(client.fd);
            client.fd = 0;
        }
    }

    if(server.fd)
    {
      close(server.fd);
      server.fd = 0;
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// cleanup - automatically called by signal
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
// Assumptions:
// State 0
//    - Waits for c.block_total clients
//
///////////////////////////////////////////////////////////////////////////////
void async_connection(int clientId, void* userData)
{
    StateVars* s = (StateVars*) userData;
    if(debug)
	    cout << "SOCKET | CALLBACK | async_connection called by client | fd=" << clientId << endl;

    if(s->state != 0) {
        if(debug)
	        cout << "** Connection limit reached: no longer accepting connections" << endl;
        return;
    }

    // Add the new client to the client list
    if (clientId)
    {
        FD_SET(clientId, &fdr);
        FD_SET(clientId	, &fdw);
        
        int send_size;
        int offset;
        init_vector.id=s->conn_count; // THIS IS IMPORTANT TO NOTE: ID assigned in order of connection
        s->conn_count++;
    
        if(s->conn_count == c.block_total && c.vertex_total%c.block_size > 0) // Edge case for last node with rows != vertex_total/block_total
        {
            send_size = c.vertex_total*(c.vertex_total-(c.block_total-1)*c.block_size); //*sizeof(int32_t); is not necessary here b/c of how adding to a memory address works.
            offset = (s->conn_count-1)*c.vertex_total*(int)ceil((1.0*c.vertex_total)/c.block_total);
        }
        else // standard case
        {
            send_size = c.vertex_total*(c.block_size); // see above
            offset = (s->conn_count-1)*c.vertex_total*c.block_size;
        }

        init_vector.data.assign(&c.adj_matrix[0]+offset,&c.adj_matrix[0]+offset+send_size);
        SendInit(clientId, init_vector);
        clients.push_back(clientId); // TODO: not fault-tolerant. make this a map later if you can!

        if(debug)
            cout << "** succesfully exchanged data with connecting client" << endl;

        if(s->conn_count == c.red_mult*c.block_total) {
            s->state=1;
            if(debug)
                cout << "** connection capacity reached | beginning computation\n\n" << endl;
            return;
        }
        
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// async_message
//
// Assumptions:
// State 1
//    - cmd_vector.c = 0
//    - req_block = true
// State 3
//    - cmd_vector.c = 1
//    - cmd_vector.data = row_k
//
///////////////////////////////////////////////////////////////////////////////
void async_message(int clientId, void* userData)
{
    StateVars* s = (StateVars*) userData;
   
    if(debug)
        cout << "SOCKET | CALLBACK | async_message called by client | fd=" << clientId << endl;

    if(s->state == 1 && s->req_block)
    { // Receive requested row, then transition to next state and send data out.
        unsigned int messageSize;
        if (ioctl(clientId, FIONREAD, &messageSize) != -1)
        {
            if(debug)
                cout << "READ | requested row_k size: " << messageSize << endl;
            if(messageSize == 0)
                return;

            // Get Message
            char* message = new char[messageSize];
            recv(clientId, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(data_vector);
            }

            // Clean up dynamically allocated memory
            delete[] message;
        
            // To reduce the size of c=0/2 commands, we'll use a temp var.
            row_k = data_vector.data;
            s->state=2;
            s->req_block=false;
            return;
        }
    }
    if(s->state==3 && s->ack_count<c.block_total*c.red_mult-1)
    {
        unsigned int messageSize;
        if (ioctl(clientId, FIONREAD, &messageSize) != -1)
        {
            if(debug)
                cout << "READ | ack message size: " << messageSize << endl;
            if(messageSize == 0)
                return;

            // Get Message
            char* message = new char[messageSize];
            recv(clientId, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(ack_msg);
            }

            if(debug)
                cout << "** ack | status=" << ack_msg.status << " | id=" << ack_msg.id << endl;
            if(ack_msg.status == 1) // && block ID not acknowledged
            {
                s->ack_count++;
                if(debug)
                    cout << "** ack count incremented | ack_count=" << s->ack_count << endl;
            }
        }        
    }
    if(s->state == 4 && s->req_block && s->k<c.vertex_total)
    {
        unsigned int messageSize;
        if (ioctl(clientId, FIONREAD, &messageSize) != -1)
        {
            if(debug)
                cout << "READ | requested solution row size: " << messageSize << endl;
            if(messageSize == 0)
                return;

            // Get Message
            char* message = new char[messageSize];
            recv(clientId, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(data_vector);
            }

            // Clean up dynamically allocated memory
            delete[] message;

            for(int i=0; i<data_vector.data.size(); i++)
            {
                solution.push_back(data_vector.data[i]);
            }
            s->k++;
            s->req_block=false;

            if(s->k == c.vertex_total)
            {   
                cmd_vector.c=2;
                cmd_vector.data.clear();
                for(int j=0;j<c.block_total*c.red_mult;j++)
                {
                    if(debug)
                        cout << "SEND | sending shutdown signal to attached client | fd=" << clients[j] << endl;	
                    SendCmd(clients[j], cmd_vector);
                }

                s->state=-1;
                return;
            }
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
    StateVars* s = (StateVars*) userData;

    if(debug)
        cout << "SOCKET | CALLBACK | async_close called" << endl;
    // Remove the clients file descriptors
    FD_CLR(clientId, &fdr);
    FD_CLR(clientId, &fdw);
    vector<int>::iterator p = find(clients.begin(), clients.end(), clientId);
    if(p != clients.end())
    {
        clients[distance(clients.begin(),p)]=0;
    }

    if(debug)
        printf("clientId: %d closed\n", clientId);
}

//////////////////////////////////////////////////////////////////////////////
//
// main_loop
//
// Assumptions:
// All fields in StateVars struct input are set.
// State 1
//    - s->k < VERTEX_TOTAL
// State 3
//    - s->req_block = false
//
///////////////////////////////////////////////////////////////////////////////
void main_loop(void* userData)
{
    StateVars* s = (StateVars*) userData;

    if(s->state==-1)
    {
        if(debug)
            cout << "\nTERMINAL STATE | computation finished" << endl;
        if(c_param==0)
        {
            if(debug)
                cout << "\nDeinfinitizing the solution ..." << endl; 
            for(int x=0;x<solution.size();x++)
            {
                if(solution[x]==INF)
                    solution[x]=0;
            }
            print_solution();
        }
        if(c_param==1)
        {
            print_solution();
        }
  
        cleanup();
        emscripten_force_exit(0);
    }
    if(s->state==0) // still waiting on connections
        return;
    if(s->state==1 && !s->req_block) // request row
    {
        if(debug)
            cout << "\nSTATE 1 | Requesting row_k, k=" << cmd_vector.k << endl;
        s->req_block=true;
        cmd_vector.c=0; // request code
        cmd_vector.k=s->k;
        cmd_vector.data.clear();
        
        SendCmd(clients[s->k/(c.block_size)],cmd_vector);   
    }
    if(s->state==2) // send row
    {   
        if(debug)
            cout << "\nSTATE 2 | Sending row_k, k=" << cmd_vector.k << endl;
        cmd_vector.c=1;
        cmd_vector.data=row_k;

        for(int j=0;j<c.block_total*c.red_mult;j++)
        {
            // k remains at the value set in state 1
            if( s->k/c.block_size==(j / c.red_mult) ) // this optimization reduces one transmission per broadcast row
                continue;
            SendCmd(clients[j], cmd_vector); // data already read directly in
        }
        s->state=3;
        if(debug)
            cout << "\nSTATE 3 | Counting completion messages" << endl;
        return;
    }
    if(s->state==3 && s->ack_count==c.block_total*c.red_mult-1) // completed counting acknolwedge msgs
    {
        if(debug)
            cout << "\nSTATE 3.5 | All completion messages received for row_k, k=" << cmd_vector.k << endl; 
        s->ack_count=0;
 
        if(s->k==c.vertex_total-1)
        {
            s->state=4; // finish up
            s->k=0;
            if(debug)
                cout << "\nSTATE 4 | Computation finshed" << endl;
            return;
        }
        else
        {
            s->state=1; // continue to iterate
            s->k++; // s->req_block should already be reset to false here
            return;
        }
    }
    if(s->state==4 && !s->req_block && s->k<c.vertex_total)
    {
        s->req_block=true;
        cmd_vector.c=0; // request code
        cmd_vector.k=s->k;
        cmd_vector.data.clear();
        
        SendCmd(clients[s->k/(c.block_size)],cmd_vector);
    }    
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
    debug=DEBUG;
    c_param=PARAM;

    // Initialize
    if(debug)
        cout << "Initializing..." << endl;

    // Mount the data folder as a NODEFS instance inside of emscripten
    EM_ASM(
        FS.mkdir('/data');
        FS.mount(NODEFS, { root: '../data' }, '/data');
    );

    string file_path="/data/";
    file_path.append(FILE_PATH);

    ifstream file(file_path);
    string ln;
    if(file.is_open())
    {
        string temp=""; // for parsing numbers
        for(int j=0; j<VERTEX_TOTAL; j++)
        {
            try
            {
                getline(file,ln);
            }
            catch(int e)
            {
                if(debug)
                    cout << "ERROR | Could not read enough lines from file" << endl;
                emscripten_force_exit(0);
            }

            for(int i=0; i<ln.length(); i++)
            {
                if(ln.at(i) == ' ')
                {
                    c.adj_matrix.push_back( atoi(temp.c_str()) );
                    temp=""; // reset the number to feed into atoi
                    continue;
                }
                temp.append(1,ln.at(i));
            }
            // gotta take care of the last number as well
            c.adj_matrix.push_back( atoi(temp.c_str()) );
            temp="";
        }
    }
    else
    {
        if(debug)
            cout << "ERROR | File could not be loaded" << endl;
        emscripten_force_exit(0);
    }

    st.state=0;
    st.conn_count=0;
    st.k=0;
    st.ack_count=0;
    st.req_block=false;
    
    c.block_total=BLOCK_TOTAL;
    c.red_mult=RED_MULT;
    c.vertex_total=VERTEX_TOTAL;

    c.block_size=ceil((c.vertex_total*1.0)/c.block_total); // NOTE: because of our use of ceil(), our block size will always produce a partition that works. The higher the tens decimal of the internal value, the more optimal our distribution our work is, with respect to the final node. 

    init_vector.v_t=c.vertex_total;
    init_vector.b_t=c.block_total;

    // Infinitize
    for(int i=0; i<c.vertex_total; i++)
    {
        for(int j=0; j<c.vertex_total; j++)
        {
            if(i!=j)
            {
                if(c.adj_matrix[i*c.vertex_total+j] == 0)
                    c.adj_matrix[i*c.vertex_total+j]=INF;
            }
        }
    }

    FD_ZERO(&fdr);
    FD_ZERO(&fdw);

    atexit(cleanup);
    signal(SIGTERM, cleanup);

    memset(&server, 0, sizeof(server_t));
    memset(&client, 0, sizeof(client_t));

    // Create the socket and set to non-blocking
    server.fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // SERVER_URL for front-end.

    if(server.fd == -1)
    {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    fcntl(server.fd, F_SETFL, O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SOCKK);

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
    emscripten_set_socket_connection_callback(&st, async_connection);
    emscripten_set_socket_message_callback(&st, async_message);
    emscripten_set_socket_close_callback(&st, async_close);

    emscripten_set_main_loop_arg(main_loop, &st, 60, 1);

    return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// SendInit
//
///////////////////////////////////////////////////////////////////////////////
static void SendInit(int clientId, const InitVector& i_vector)
{
    if(debug)
        cout << "SEND | Sending client " << clientId << " initialization data" << endl;

    int res;

    if (!FD_ISSET(clientId, &fdw))
    {
        if(debug)
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
            if(debug)
                cout << "ERROR: Exception thrown sending data to clientId " << clientId << endl;
            return;
        }
        else if (res == 0)
        {
            // Client disconnected
            memset(&client, 0, sizeof(client_t));
            return;
        }

        if(debug)
            cout << "** sent " << tmp.size() << " bytes to client " << clientId << endl;
    }
    catch (...)
    {
        if(debug)
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
    if(debug)
        cout << "SEND | SendCmd called | code=" << cmd_vector.c << " | k=" << cmd_vector.k << endl;

    int res;

    if (!FD_ISSET(clientId, &fdw))
    {
        if(debug)
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
            if(debug)
                cout << "ERROR: Exception thrown sending data to clientId " << clientId << endl;
            return;
        }
        else if (res == 0)
        {
            // Client disconnected
            memset(&client, 0, sizeof(client_t));
            return;
        }

        if(debug)
            cout << "** sent " << tmp.size() << " bytes to client " << clientId << endl;
    }
    catch (...)
    {
        if(debug)
            cout << "ERROR: could not serialize the display data" << endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// print_solution
//
///////////////////////////////////////////////////////////////////////////////
static void print_solution()
{
    int n=c.vertex_total;
    for(int j=0; j<solution.size(); j++)
    {
        if(j!=0 && j%n==0)
            printf("\n");
        printf("%d ",solution[j]);
    }
    printf("\n\n");
}
