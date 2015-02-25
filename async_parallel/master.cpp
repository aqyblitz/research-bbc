// All the necessary imports here.
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <vector>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 8888

using namespace std;

// TODO
// - we're sending the data back to all the sockets for relaxation...including the one we got it from. Nah.

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

void setup(const int& argc, char** argv, fd_set& readfds, int32_t& master_socket, sockaddr_in& address, int32_t& address_len, int32_t* client_socket, int32_t& block_total, int32_t& red_mult, int32_t& vertex_total, int32_t& block_size, int32_t& max_clients);
void teardown();
bool emscripten_main(fd_set& readfds, const sockaddr_in& address, const int32_t& address_len, int32_t& master_socket, int32_t* client_socket, timeval& tv, int32_t& max_sd, int32_t& s, vector<int32_t>& adj_matrix, vector<int32_t>& row_k, int32_t& k, int32_t& ack_slave_c, int32_t& slave_count, const int32_t& slave_total, const int32_t& vertex_total, const int32_t& max_clients, const int32_t& block_size, bool& requested, int32_t& ack);

int main(int argc, char *argv[])
{
    fd_set readfds;
    int32_t block_total, red_mult, vertex_total, block_size;
    int32_t master_socket , addrlen , new_socket , max_clients;
    int32_t client_socket[1000]; //temp cap, compare with [block_total*red_mult+1]
    int32_t max_sd;
    struct sockaddr_in address;
    int32_t address_len;
    struct timeval tv = {.5, 0};

    // [DEBUG]are
    int32_t hard[49] = {1,1,0,0,1,0,0, 1,0,1,0,1,0,0, 0,1,0,1,0,0,0, 0,0,1,0,1,1,0, 1,1,0,1,0,0,0, 0,0,0,1,0,0,0, 0,0,0,0,0,0,0};
    //int32_t hard[16] = {0,0,2,0, 4,0,3,0, 0,0,0,2, 0,1,0,0};
    vector<int32_t> adj_matrix(hard, hard+sizeof(hard)/sizeof(int32_t) ); //generate_adj_matrix(vertex_total);
    vector<int32_t> row_k(vertex_total);

    setup(argc, argv, readfds, master_socket, address, address_len, client_socket, block_total, red_mult, vertex_total, block_size, max_clients);

    bool loop=true;
    int32_t state=0;
    int32_t slave_count=0;
    int32_t k=0;
    bool requested=false;
    int32_t ack=0;
    int32_t ack_slave_c=0;

    while(loop)
    {
        loop=emscripten_main(readfds,address,address_len,master_socket,client_socket,tv,max_sd,state,adj_matrix,row_k,k,ack_slave_c,slave_count,block_total,vertex_total,block_size,max_clients,requested,ack);
    }

    // TODO doing anything here creates:
    /*
     * terminate called after throwing an instance of 'std::bad_alloc'
     *  what():  std::bad_alloc */

    teardown();

    return 0;
}

void setup(const int& argc, char** argv, fd_set& readfds, int32_t& master_socket, sockaddr_in& address, int32_t& address_len, int32_t* client_socket, int32_t& block_total, int32_t& red_mult, int32_t& vertex_total, int32_t& block_size, int32_t& max_clients)
{
    int32_t i;
    int32_t opt=1; // aka TRUE
    if(argc != 4)
    {
        fprintf(stderr,"ERROR: Usage <# of blocks> <redundancy multiplier> <# of slave nodes per block> <total vertex count>\n");
        exit(1);
    }

    block_total = atoi(argv[1]); // number of blocks of data
    red_mult = atoi(argv[2]); // redundancy multiplier
    vertex_total = atoi(argv[3]); // suuuper important
    block_size = vertex_total/block_total; // ** NUMBER OF ROWS STOP FORGETTING **
    max_clients = red_mult*block_total;
    if(max_clients > 1000-1)
    {
        fprintf(stderr,"ERROR: Client limit exceeded\n");
        exit(1);
    }

    //client_socket = new int32_t[max_clients+1];
    // Setup socket stuff.
    // Initialize to 0. Means they're empty (for now).
    for (i=0; i<max_clients+1; i++) // TODO remove the +1
    {
        client_socket[i] = 0;
    }

    // Initialize master socket. Set options to allow multiple ocnnections.
    if((master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    fcntl(master_socket, F_SETFL, O_NONBLOCK);

    // Set values on address struct.
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
    address_len = sizeof(address);

    // Bind the socket to HOST, PORT
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // Specify maximum of 3 pending connections for the master socket.
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

void teardown()
{

}

void build_fd_table(fd_set& fds, int32_t& master_socket, int32_t* client_socket, const int32_t& max_clients, int32_t& max_sd)
{
    int sd;

    // Zero the fd set.
    FD_ZERO(&fds);

    // Add master socket to set
    FD_SET(master_socket, &fds);
    max_sd = master_socket;

    // Add child sockets to set
    for (int i=0 ; i<max_clients ; i++)
    {
        sd=client_socket[i];

        // If valid, add to read list
        if(sd > 0)
            FD_SET(sd,&fds);

        // Need max_sd for select
        if(sd > max_sd)
            max_sd = sd;
    }
}

bool emscripten_main(fd_set& readfds, const sockaddr_in& address, const int32_t& address_len, int32_t& master_socket, int32_t* client_socket, timeval& tv, int32_t& max_sd, int32_t &s, vector<int32_t>& adj_matrix, vector<int32_t>& row_k, int32_t& k, int32_t& ack_slave_c, int32_t& slave_count, const int32_t& block_total, const int32_t& vertex_total, const int32_t& max_clients, const int32_t& block_size, bool& requested, int32_t& ack)
{
    int32_t i, n, sd, new_socket, activity;

    if(s==0) // state 0
    {
        //puts("[DEBUG] STATE 0");
        build_fd_table(readfds,master_socket,client_socket,max_clients,max_sd);
        //puts("[DEBUG] FD table built");

        activity = select( max_sd + 1 , &readfds , NULL , NULL , &tv);

        if(activity>0)
        {
            puts("[DEBUG] Activity detected ...");
            if(FD_ISSET(master_socket, &readfds)) // incoming connection
            {
                puts("[DEBUG] ... on master socket");
                if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&address_len))<0)
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                fcntl(new_socket, F_SETFL, O_NONBLOCK);

                // Increment count of number of slaves we'll have to work with
                slave_count++;

                printf("[LOG] New connection: socket fd=%d , ip=%s , port=%d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                // Prepping firwell it dost message.
                int32_t size_sizes = 4;
                int32_t sizes[size_sizes];
                sizes[0] = vertex_total;
                sizes[2] = slave_count-1; //setting ID. This will always work, but the harder part of redundancy is incrementing slave_count properly.
                sizes[3] = block_total;

                int32_t send_size;
                if(slave_count == block_total && vertex_total%block_size > 0)
                {
                    puts ("[DEBUG] Sending data for unique end case");
                    send_size = vertex_total*(vertex_total-(block_total-1)*block_size)*sizeof(int32_t);
                    sizes[1] = send_size;

                    if( send(new_socket, sizes, sizeof(int32_t)*size_sizes, 0) != sizeof(int32_t)*size_sizes )
                    {
                        perror("send");
                    }
                    puts("[DEBUG] Sent metadata");
                    if( send(new_socket, &adj_matrix[0]+(slave_count-1)*vertex_total*(int)ceil((1.0*vertex_total)/block_total), send_size, 0) != send_size )
                    {
                        perror("send");
                    }
                    puts("[DEBUG] Sent row data");
                }
                else
                {
                    puts ("[DEBUG] Sending data for standard case");
                    send_size = vertex_total*(block_size)*sizeof(int32_t);
                    sizes[1] = send_size;

                    if( send(new_socket, sizes, sizeof(int32_t)*size_sizes, 0) != sizeof(int32_t)*size_sizes )
                    {
                        perror("send");
                    }
                    puts("[DEBUG] Sent metadata");
                    if( send(new_socket, &adj_matrix[0]+(slave_count-1)*vertex_total*block_size, send_size, 0) != send_size )
                    {
                        perror("send");
                    }
                    puts("[DEBUG] Sent row data");
                }

                printf("[LOG] Data sent successfully to slave node id=%d\n",slave_count-1);

                //add new socket to array of sockets
                for (i=0; i < max_clients; i++)
                {
                    if( client_socket[i] == 0 )
                    {
                        client_socket[i] = new_socket;
                        printf("Adding to list of sockets as %d\n" , i);
                        break;
                    }
                }
            }

            if(slave_count == block_total+1) // [RED] +1 for now
            {
                s=1;
                // DEBUG
                build_fd_table(readfds,master_socket,client_socket,max_clients,max_sd);
                return true;
            }
        }
    }

    if(s==1)
    {
        //puts("[DEBUG] STATE 1");
        if(k==vertex_total)
        {
            s=5;
            k=0; // going to re-use for retrieving answer
            requested=false;
            return true;
        }

        if(!requested)
        {
            // Request row k
            printf("[DEBUG] Retrieve row k=%d\n",k);
            //printf("[DEBUG] ID=%d\n",k/(block_size));
            sd = client_socket[k/(block_size)];
            //printf("[DEBUG] SOCK_FD=%d\n",client_socket[k/(block_size)]);
            int sock_code = FD_ISSET(sd, &readfds);
            if(sock_code)
            {
                int32_t size_req = 2;
                int32_t req[size_req];

                // Send command 0 (request)
                req[0] = 0;
                req[1] = k;

                // Command 0 sent (request row)
                printf("[DEBUG] Command %d: k=%d\n",req[0],req[1]);
                printf("[DEBUG] Requesting row %d from slave ID=%d\n",k,k/block_size);
                if( send(sd, &req, sizeof(int32_t)*size_req, 0) != sizeof(int32_t)*size_req )
                {
                    perror("send");
                }
                //printf("Socket Descriptor: %d\n",sd);
                requested=true;
            }
            else
            {
                printf("-- ERROR: Socket ID %d not valid connection on REQUEST command | %d\n",k/block_size,sock_code);
            }
        }
        else
        {
            n=read(sd, &row_k[0],sizeof(int32_t)*vertex_total);
            if(n != -1)
                printf("%d\n",n);
            if(n == vertex_total*sizeof(int32_t))
            {
                s=2;
                print_adj_matrix(row_k,vertex_total);
                return false;
            }
        }
    }

    if(s==2)
    {
        build_fd_table(readfds,master_socket,client_socket,max_clients,max_sd);
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
            if(ack==block_total) // [RED]
            {
                k++;
                ack=0;
                ack_slave_c=0;
                requested=false;
                s=1;
                return true;
            }
        }
    }

    if(s==5)
    {
        if(k==vertex_total)
        {
            return false;
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

    return true;
}
