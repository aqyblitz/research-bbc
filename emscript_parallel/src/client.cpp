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
void main_loop();

// Structs
typedef struct {
  int fd;
} server_t;

typedef struct {
  int32_t v_t; // # of vertices in graph
  int32_t id; // int id of this node. Serves as block identifier for now.
  int32_t b_t; // # of distinct blocks 
  int32_t b_s; // # of rows in this block
} Constants;

typedef struct {
  int state; // encodes overall state of client
  int cmd_s; // cmd state: 0 or 1
  int k; // row being requested o relaxed w/ respect to
  int base_offset; // base_offset
  int r_d; // # of rows relaxed
} StateVars;

// Global variables
server_t       server;
Constants      c;
StateVars      st;

vector<int32_t> block; // data block

InitVector   init_vector; // For receiving initialization data.
CmdVector    cmd_vector;  // For receiving commands.
DataVector   data_vector; // For sending requested row back to server.
AckMsg       ack_msg;     // For sending acknowledge message back to server.

std::string server_info = "http://localhost:8888";

void finish(int result) {
  if (server.fd)
  {
    std::cout << "closing server.fd " << server.fd << " with status " << result << std::endl;
    close(server.fd);
    server.fd = 0;
  }

  emscripten_force_exit(0);
}

///////////////////////////////////////////////////////////////////////////////
//
// async_message
//
///////////////////////////////////////////////////////////////////////////////
void async_message(int fd, void* userData)
{ 
    StateVars* s = (StateVars*) userData;

    cout << "\nSOCKET | CALLBACK | async_message called at state " << s->state << endl;

    if(s->state==0)
    {
        // post-connection, receive data
        unsigned int messageSize;
        if (ioctl(fd, FIONREAD, &messageSize) != -1)
        {
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
            // cout << c.v_t << " " << c.id << " " << c.b_t << endl; // ** DEBUG
            print_block();  
        }

        s->state=1; // only ingest commands
        cout << "** Client set to process commands\n" << endl;
        return;
    }
    if(s->state==1)
    {
        // Ingest command.
        unsigned int messageSize;
        if (ioctl(fd, FIONREAD, &messageSize) != -1)
        {
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
            cout << "** CMD | code=" << s->cmd_s << " | k=" << s->k << endl;
            s->base_offset=c.v_t*(s->k-(c.id*(int)ceil( (c.v_t*1.0)/c.b_t))); 

            if(s->cmd_s==0) // code=0, data requested --> send it back
            {
                // Send data.
                vector<int32_t> temp_data(block.begin()+s->base_offset,block.begin()+s->base_offset+c.v_t);
                data_vector.data=temp_data;

                SendData(data_vector);
            }
            if(s->cmd_s==1) // code=1, data received, relax local data --> send ack
            {
                /*
                for(int i=0; i<cmd_vector.data.size(); i++)
                {
                    cout << cmd_vector.data[i] << " ";
                }
                cout << endl;
                */
                int ack_c;//=0; // 0 for failure
                relax(cmd_vector.data, s->k);
                s->r_d++; // increment rows done TODO: front end
                ack_c=1; // 1 for success
                
                // SendAckMsg
                ack_msg.status=ack_c;
                ack_msg.id=c.id;
                cout << "****** ACK | status=" << ack_msg.status << " | id=" << ack_msg.id << endl;
                SendAckMsg(ack_msg);
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
    cout << "SOCKET | CALLBACK | async_close called" << endl;
}

void error_callback(int fd, int err, const char* msg, void* userData) {
  int error;
  socklen_t len = sizeof(error);

  int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
  printf("%s callback\n", userData);
  printf("error message: %s\n", msg);

  if (err == error) {
    finish(EXIT_SUCCESS);
  } else {
    finish(EXIT_FAILURE);
  }
}

void main_loop(void* userData) {}

int main() {
  char str[INET_ADDRSTRLEN];
  struct hostent *host = NULL;
  struct hostent hostData;
  struct sockaddr_in addr;
  const char *res;
  char buffer[2048];
  int err;
  int res2;

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
  
  // gethostbyname_r calls the same stuff as gethostbyname, so we'll test the
  // more complicated one.
  // resolve the hostname to an actual address
  gethostbyname_r(HOST_NAME, &hostData, buffer, sizeof(buffer), &host, &err); // HOSTNAME HARDCODED TO LOCALHOST
  //assert(h-ost->h_addrtype == AF_INET);
  //assert(host->h_length == sizeof(uint32_t));

  // convert the raw address to a string
  char **raw_addr_list = host->h_addr_list;
  int *raw_addr = (int*)*raw_addr_list;
  res = inet_ntop(host->h_addrtype, raw_addr, str, INET_ADDRSTRLEN);
  //assert(res);

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

  emscripten_set_main_loop_arg(main_loop, &st, 60, 1); // had to add this to keep state struct in scope for some reason...

  return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// SendData
//
///////////////////////////////////////////////////////////////////////////////
static void SendData(const DataVector& d_vector)
{
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
            cout << "ERROR: Exception thrown sending data to server " << endl;
            return;
        }
        else if (res == 0)
        {
            // Server disconnected
            return;
        }

        cout << "** sent requested data: " << tmp.size() << " bytes to server " << endl;
    }
    catch (...)
    {
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
            cout << "ERROR: Exception thrown sending ack to server " << endl;
            return;
        }
        else if (res == 0)
        {
            // Server disconnected
            return;
        }

        cout << "** sent completion message: " << tmp.size() << " bytes to server" << endl;
    }
    catch (...)
    {
        cout << "ERROR: could not serialize ack message" << endl;
    }
}

static void relax(vector<int32_t> &row_k, int k)
{
    cout << "**** Relaxing data" << endl;
    int ind;
    int comp_distance;

    for(int i=0;i<c.b_s;i++)
    {
        for(int j=0;j<c.v_t;j++)
        {
            ind=i*c.v_t+j;
            comp_distance = block[i*c.v_t+k]+row_k[j]; // a_ik+a_jk
            //cout << "** COMP | a_ik+a_jk=" << comp_distance << " | a_ij=" << block[ind] << endl;

            if(block[i*c.v_t+k] == INF || row_k[j] == INF)
                continue;
            if(block[ind] > comp_distance)
                block[ind] = comp_distance;
        }
    }
    cout << "**** Relaxation complete" << endl;
    //print_block();
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
