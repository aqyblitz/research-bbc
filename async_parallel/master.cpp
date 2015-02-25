// All the necessary imports here.
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <vector>
#include <cstdlib>
#include <stdio.h>

#define PORT 8888

using namespace std;

// TODO
// - we're sending the data back to all the sockets for relaxation...including the one we got it from. Nah.

void print_adj_matrix(vector<int32_t> matrix, int n)
{
    for(int j=0; j<matrix.size(); j++)
    {
        if(j!=0 && j%n==0)
            printf("\n");
        printf("%d ",matrix[j]);
    }
    printf("\n");
}

void setup(const int& argc, char** argv, fd_set& readfds, fd_set& copyfds, int32_t& master_socket, sockaddr_in& address, int32_t& address_len, int32_t* client_socket, int32_t& block_total, int32_t& red_mult, int32_t& vertex_total, int32_t& block_size, int32_t& max_clients);
void teardown(int32_t* client_socket);
void emscripten_main(fd_set& readfds, fd_set& copyfds, const sockaddr_in& address, const int32_t& address_len, int32_t& master_socket, int32_t* client_socket, int32_t& max_sd, int32_t& s, int32_t& k, int32_t& ack_slave_c, int32_t& slave_count, const int32_t& slave_total, const int32_t& vertex_total, const int32_t& max_clients, bool& requested, int32_t& ack, bool& loop);

int main(int argc, char *argv[])
{
    fd_set readfds, copyfds;
    int32_t block_total, red_mult, vertex_total, block_size;
    int32_t master_socket , addrlen , new_socket , max_clients;
    int32_t* client_socket; //[block_total*red_mult+1]
    int32_t max_sd;
    struct sockaddr_in address;
    int32_t address_len;

    setup(argc, argv, readfds, copyfds, master_socket, address, address_len, client_socket, block_total, red_mult, vertex_total, block_size, max_clients);

    bool loop=false;
    int32_t state=0;
    int32_t slave_count=0;
    int32_t k=0;
    bool requested=false;
    int32_t ack=0;
    int32_t ack_slave_c=0;

    vector<int32_t> solution(vertex_total*vertex_total);

    while(loop)
    {
        emscripten_main(readfds,copyfds,address,address_len,master_socket,client_socket,max_sd,state,k,ack_slave_c,slave_count,block_total,vertex_total,max_clients,requested,ack,loop);
    }

    teardown(client_socket);

    return 0;
}

void setup(const int& argc, char** argv, fd_set& readfds, fd_set& copyfds, int32_t& master_socket, sockaddr_in& address, int32_t& address_len, int32_t* client_socket, int32_t& block_total, int32_t& red_mult, int32_t& vertex_total, int32_t& block_size, int32_t& max_clients)
{
    int32_t i;
    int32_t opt=1; // aka TRUE
    if(argc != 4)
    {
        fprintf(stderr,"ERROR: Input should come in the form of two int values: # of blocks, # of slave nodes per block\n");
        exit(1);
    }

    block_total = atoi(argv[1]); // number of blocks of data
    red_mult = atoi(argv[2]); // redundancy multiplier
    vertex_total = atoi(argv[3]); // suuuper important
    block_size = vertex_total/block_total; // ** NUMBER OF ROWS STOP FORGETTING **
    max_clients = red_mult*block_total;

    client_socket = new int32_t[max_clients+1];
    // Setup socket stuff.
    // Initialize to 0. Means they're empty (for now).
    for (i=0; i<max_clients; i++)
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

    // Initialize arrays.

}

void teardown(int* client_socket)
{
    // Close connections.
    // Delete stuff allocated with new.
    delete[] client_socket;
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

void rebuild_fd_table(fd_set& readfds, fd_set& copyfds)
{
    // Deep copy
}

// int32_t max_sd,
void emscripten_main(fd_set& readfds, fd_set& copyfds, const sockaddr_in& address, const int32_t& address_len, int32_t& master_socket, int32_t* client_socket, int32_t& max_sd, int32_t &s, int32_t& k, int32_t& ack_slave_c, int32_t& slave_count, const int32_t& block_total, const int32_t& vertex_total, const int32_t& max_clients, bool& requested, int32_t& ack, bool& loop)
{
    int32_t i, n, sd, new_socket, activity;

    if(s==0) // state 0
    {
        rebuild_fd_table(readfds,copyfds);
        //activity=select(); // poll sockets
        if(activity>0)
        {
            for(i=0; i<max_clients; i++)// all connections
            {
                //new_socket = accept();
                if(FD_ISSET(master_socket, &readfds)) // incoming connection
                {
                    if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&address_len))<0)
                    {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }

                    // Increment count of number of slaves we'll have to work with
                    slave_count++;

                    printf("New connection: socket fd=%d , ip=%s , port=%d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    // Prepping first message.
                    int32_t size_sizes = 4;
                    int32_t sizes[size_sizes];
                    sizes[0] = vertex_total;
                    sizes[2] = slave_count-1; //setting ID. This will always work, but the harder part of redundancy is incrementing slave_count properly.
                    sizes[3] = block_total;

                    int32_t send_size0;
                    if(slave_count == vertex_total && vertex_total%block_total>0)
                    {
                        puts ("** NON-EVEN CASE (unique end case)");
                        send_size = vertex_total*(vertex_total%block_total)*sizeof(int32_t);
                        sizes[1] = send_size;

                        if( send(new_socket, sizes, sizeof(int32_t)*size_sizes, 0) != sizeof(int32_t)*size_sizes )
                        {
                            perror("send");
                        }
                        puts("** SENT SIZE OF ROW DATA TO RECEIVE");
                        if( send(new_socket, &adj_matrix[0]+(slave_count-1)*vertex_total*(vertex_total%block_total), send_size, 0) != send_size )
                        {
                            perror("send");
                        }
                        puts("** SENT ROW DATA");
                    }
                    else
                    {
                        puts ("** EVEN CASE");
                        send_size = vertex_total*(block_size)*sizeof(int32_t);
                        sizes[1] = send_size;

                        if( send(new_socket, sizes, sizeof(int32_t)*size_sizes, 0) != sizeof(int32_t)*size_sizes )
                        {
                            perror("send");
                        }
                        puts("** SENT SIZE OF ROW DATA TO RECEIVE");
                        if( send(new_socket, &adj_matrix[0]+(slave_count-1)*vertex_total*block_size, send_size, 0) != send_size )
                        {
                            perror("send");
                        }
                        puts("** SENT ROW DATA");
                    }

                    printf("Data sent successfully to slave node %d\n",slave_count-1);

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

                if(slave_count == block_total) // [RED]
                {
                    s=1;
                    // DEBUG
                    loop=false;
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
        rebuild_fd_table(readfds,copyfds);
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