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
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace std;

#include "include/structs.h"

void error_callback(int fd, int err, const char* msg, void* userData);

typedef struct {
  int fd;
} server_t;

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <stdio.h>
#include <vector>
#include <string>

#include "include/structs.h"

void main_loop();

server_t server;

int state;			// TODO: state struct
bool setup;			// TODO: state struct
bool sent;			// TODO: state struct

std::string server_info = "http://localhost:8888";

void finish(int result) {
  if (server.fd) {
    std::cout << "closing server.fd " << server.fd << " with status " << result << std::endl;
    close(server.fd);
    server.fd = 0;
  }
#ifdef __EMSCRIPTEN__
  emscripten_force_exit();
#else
  exit();
#endif
}

static int get_index(int i, int j, int w)
{
    return i*w+j;
}

void relax(vector<int32_t> &adj_block, int id, vector<int32_t> &row_k, int k, int b_s, int v_t)
{
    int ind;
    int comp_distance;
    for(int i=0;i<b_s;i++)
    {
        for(int j=0;j<v_t;j++)
        {
            ind=get_index(i,j,v_t);
            comp_distance = adj_block[get_index(i,k,v_t)]+row_k[j];
            if(adj_block[ind] > comp_distance)
                adj_block[ind] = comp_distance;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// async_message
//
///////////////////////////////////////////////////////////////////////////////
void async_message(int clientId, void* userData)
{
    if(s==0 && setup)
    {
        // post-connection, receive data
        // set constants
        s=1; // only ingest commands
    }
    if(s==1)
    {
        // Ingest command.
        if(/*code==0 &&*/)
        {
        // SendData
        }
        if(/*code==1 &&*/)
        {
            // relax(row_k);
            sent=false;
            // SendAckMsg
        } 
    }
}

void main_loop() {

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

int main() {
  char str[INET_ADDRSTRLEN];
  struct hostent *host = NULL;
  struct hostent hostData;
  struct sockaddr_in addr;
  const char *res;
  char buffer[2048];
  //int err;
  int res2;

  state=0;
  setup=false;
  sent=false;

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
  gethostbyname_r(HOST_NAME, &hostData, buffer, sizeof(buffer), &host, &err);
  //assert(host->h_addrtype == AF_INET);
  //assert(host->h_length == sizeof(uint32_t));

  // convert the raw address to a string
  char **raw_addr_list = host->h_addr_list;
  int *raw_addr = (int*)*raw_addr_list;
  res = inet_ntop(host->h_addrtype, raw_addr, str, INET_ADDRSTRLEN);
  //assert(res);
   // von
  if (inet_pton(AF_INET, str, &addr.sin_addr) != 1) { // convert text IP to inary
    perror("inet_pton failed");
    finish(EXIT_FAILURE);
  }

  res2 = connect(server.fd, (struct sockaddr *)&addr, sizeof(addr));
  if (res2 == -1 && errno != EINPROGRESS) {
    perror("connect failed");
    finish(EXIT_FAILURE);
  }

  emscripten_set_socket_message_callback((void *) NULL, async_message);

  //emscripten_set_main_loop(main_loop, 60, 1);

  return EXIT_SUCCESS;
}
