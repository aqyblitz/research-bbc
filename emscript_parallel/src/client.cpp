#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <iostream>
#include <netdb.h>
#include <stdio.h>
#include <vector>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <emscripten/bind.h>

#include "include/structs.h"

// Definitions
#define INF 1000

// Namespace
using namespace std;

// Function signatures
static void print_block();
static void SendData   (const DataVector& d_vector);
static void SendAckMsg (const AckMsg&     a_msg);
static void relax      (vector<int32_t> &row_k, int k);
void error_callback(int fd, int err, const char* msg, void* userData);

// Structs
typedef struct {
    int fd;
} server_t;

typedef struct {
    int32_t v_t;       // # of vertices in graph
    int32_t id;        // int id of this node. Serves as block identifier for now.
    int32_t b_t;       // # of distinct blocks 
    int32_t b_s;       // # of rows in this block
} Constants;

typedef struct {
    int state;         // encodes overall state of client
    int cmd_s;         // cmd state: 0 or 1
    int k;             // row being requested o relaxed w/ respect to
    int base_offset;   // base_offset
    int r_d;           // # of rows relaxed
} StateVars;

// Global variables
server_t           server;
Constants          c;
StateVars          st;

bool               debug;
int                c_param;
vector<int32_t>    block; // data partition

InitVector         init_vector; // For receiving initialization data.
CmdVector          cmd_vector;  // For receiving commands.
DataVector         data_vector; // For sending requested row back to server.
AckMsg             ack_msg;     // For sending acknowledge message back to server.

void finish(int result) {
    if (server.fd)
    {
      if(debug)
          cout << "closing server.fd " << server.fd << " with status " << result << endl;
      close(server.fd);
      server.fd = 0;
    }

    emscripten_force_exit(0);
}

///////////////////////////////////////////////////////////////////////////////
//
// async_message
//
// Assumptions:
// State 0
//    - init_vector correctly assigned in server
// State 1
//    - 0 <= cmd_vector.c <= 2
//
///////////////////////////////////////////////////////////////////////////////
void async_message(int fd, void* userData)
{ 
    StateVars* s = (StateVars*) userData;

    if(debug)
        cout << "\nSOCKET | CALLBACK | async_message called at state " << s->state << endl;

    if(s->state==0)
    {
        // post-connection, receive data
        unsigned int messageSize;
        if (ioctl(fd, FIONREAD, &messageSize) != -1)
        {
            if(debug)
                cout << "READ | Receiving init data from server | InitVector size: " << messageSize << endl;
            if(messageSize == 0)
                return;

            // Get Message
            char* message = new char[messageSize];
            recv(fd, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(init_vector);
            }
            // Clean up dynamically allocated memory
            delete[] message;

            c.v_t=init_vector.v_t;
            c.id=init_vector.id;
            c.b_t=init_vector.b_t;
            block=init_vector.data;
            c.b_s=block.size()/c.v_t;

            if(debug)
                print_block();  
        }

        s->state=1; // only ingest commands
        if(debug)
            cout << "** Client set to process commands\n" << endl;
        return;
    }
    if(s->state==1)
    {
        // Ingest command.
        unsigned int messageSize;
        if (ioctl(fd, FIONREAD, &messageSize) != -1)
        {
            if(debug)
                cout << "READ | CommandVector Size: " << messageSize << endl;
            if(messageSize == 0)
                return;

            // Get Message
            char* message = new char[messageSize];
            recv(fd, message, messageSize, 0);

            // De-serialize the data
            {
                std::stringstream ss;
                ss.write(message, messageSize);

                cereal::PortableBinaryInputArchive archive(ss);
                archive(cmd_vector);
            }
            // Clean up dynamically allocated memory
            delete[] message;
     
            // Read values.
            s->cmd_s=cmd_vector.c;
            s->k=cmd_vector.k;

            if(debug)
                cout << "** CMD | code=" << s->cmd_s << " | k=" << s->k << endl;
            s->base_offset=c.v_t*(s->k-(c.id*(int)ceil( (c.v_t*1.0)/c.b_t))); 

            if(s->cmd_s==0) // code=0, data requested --> send it back
            {
                // Send data.
                vector<int32_t> temp_data(block.begin()+s->base_offset,block.begin()+s->base_offset+c.v_t);
                data_vector.data=temp_data;

                SendData(data_vector);
                if(s->r_d != c.b_s/c.v_t) 
                {
                    relax(temp_data, s->k); // this relaxes the local data
                    s->r_d++; // increment rows done TODO: front end
                }

                return;
            }
            if(s->cmd_s==1) // code=1, data received OR this is the source --> relax local data --> send ack
            {
                int ack_c;//=0; // 0 for failure
                relax(cmd_vector.data, s->k); // this k-value will work for both cases
                s->r_d++; // increment rows done TODO: front end
                ack_c=1; // 1 for success
                
                // SendAckMsg
                ack_msg.status=ack_c;
                ack_msg.id=c.id;
                if(debug)
                    cout << "****** ACK | status=" << ack_msg.status << " | id=" << ack_msg.id << endl;
                SendAckMsg(ack_msg);
                
                return;
            }
            if(s->cmd_s==2) // code=2, shut down
            {
                finish(1); // TODO: exit codes
            }
        }     
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// async_close
//
///////////////////////////////////////////////////////////////////////////////
void async_close(int fd, void* userData)
{
    StateVars* s = (StateVars*) userData;
    if(debug)
      cout << "SOCKET | CALLBACK | async_close called" << endl;
}

void error_callback(int fd, int err, const char* msg, void* userData) {
    int error;
    socklen_t len = sizeof(error);

    int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if(debug)
    {
        printf("%s callback\n", userData);
        printf("error message: %s\n", msg);
    }

    if (err == error) {
        finish(EXIT_SUCCESS);
    } else {
        finish(EXIT_FAILURE);
    }
}

int main() {
    char str[INET_ADDRSTRLEN];
    struct hostent *host = NULL;
    struct hostent hostData;
    struct sockaddr_in addr;
    const char *res;
    char buffer[2048];
    int err;
    int res2;
    debug=DEBUG;
    c_param=PARAM;
  
    st.state=0;
    st.r_d=0;

    memset(&server, 0, sizeof(server_t));

    // create the socket and set to non-blocking
    server.fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  
    if (server.fd == -1) {
      perror("cannot create socket");
      finish(EXIT_FAILURE);
    }
    fcntl(server.fd, F_SETFL, O_NONBLOCK);

    // connect the socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SOCKK);
  
    // resolve the hostname to an address
    gethostbyname_r(HOST_NAME, &hostData, buffer, sizeof(buffer), &host, &err); // HOSTNAME HARDCODED TO LOCALHOST

    // convert the raw address to a string
    char **raw_addr_list = host->h_addr_list;
    int *raw_addr = (int*)*raw_addr_list;
    res = inet_ntop(host->h_addrtype, raw_addr, str, INET_ADDRSTRLEN);

    if (inet_pton(AF_INET, str, &addr.sin_addr) != 1) { // convert text IP to binary
      perror("inet_pton failed");
      finish(EXIT_FAILURE);
    }

    res2 = connect(server.fd, (struct sockaddr *)&addr, sizeof(addr));
    if (res2 == -1 && errno != EINPROGRESS) {
      perror("connect failed");
      finish(EXIT_FAILURE);
    }

    emscripten_set_socket_message_callback(&st, async_message);
    emscripten_set_socket_close_callback(&st, async_close);

    return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// SendData
//
///////////////////////////////////////////////////////////////////////////////
static void SendData(const DataVector& d_vector)
{
    if(debug)
        cout << "SEND | Sending requested row_k" << endl;

    int res;
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
        res = send(server.fd, message, tmp.size(), 0);

        if (res == -1) {
            if(debug)
                cout << "ERROR: Exception thrown sending data to server " << endl;
            return;
        }
        else if (res == 0)
        {
            // Server disconnected
            return;
        }

        if(debug)
            cout << "** sent requested data: " << tmp.size() << " bytes to server " << endl;
    }
    catch (...)
    {
        if(debug)
            cout << "ERROR: could not serialize row_k data" << endl;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// SendAckMsg
//
///////////////////////////////////////////////////////////////////////////////
static void SendAckMsg(const AckMsg& a_msg)
{
    if(debug)
        cout << "SEND | completion message sending" << endl;

    int res;
    // Serialize the data
    try
    {
        std::stringstream ss;
        cereal::PortableBinaryOutputArchive archive(ss);
        archive(a_msg);

        // Copy the stringstream to a char*
        const std::string& tmp = ss.str();
        const char* message = tmp.c_str();

        // Send the message
        res = send(server.fd, message, tmp.size(), 0);

        if (res == -1) {
            if(debug)
                cout << "ERROR: Exception thrown sending ack to server " << endl;
            return;
        }
        else if (res == 0)
        {
            // Server disconnected
            return;
        }

        if(debug)
            cout << "** sent completion message: " << tmp.size() << " bytes to server" << endl;
    }
    catch (...)
    {
        if(debug)
            cout << "ERROR: could not serialize ack message" << endl;
    }
}

static void relax(vector<int32_t> &row_k, int k)
{
    if(debug)
        cout << "**** Relaxing data" << endl;
    int ind;
    int comp_distance;

    for(int i=0;i<c.b_s;i++)
    {
        for(int j=0;j<c.v_t;j++)
        {
            ind=i*c.v_t+j;
            comp_distance = block[i*c.v_t+k]+row_k[j]; // a_ik+a_jk

            if(block[i*c.v_t+k] == INF || row_k[j] == INF)
                continue;
            if(block[ind] > comp_distance)
                block[ind] = comp_distance;
        }
    }
    if(debug)
        cout << "**** Relaxation complete" << endl;
}

///////////////////////////////////////////////////////////////////////////////
//
// print_block
//
///////////////////////////////////////////////////////////////////////////////
static void print_block()
{
    int n=c.v_t;
    for(int j=0; j<block.size(); j++)
    {
        if(j!=0 && j%n==0)
            printf("\n");
        printf("%d ",block[j]);
    }
    printf("\n");
}
