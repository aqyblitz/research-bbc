#include <stdio.h>
#include <stdint.h>
#include <string.h>   //strlen
#include <vector>
#include <stdlib.h>
#include <cstdlib>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <ctime>

#define TRUE   1
#define FALSE  0
#define PORT 8888

/*
  FD_ZERO - Clear an fd_set
  FD_ISSET - Check if a descriptor is in an fd_set
  FD_SET - Add a descriptor to an fd_set
  FD_CLR - Remove a descriptor from an fd_set
*/

using namespace std;

vector<int32_t> generate_adj_matrix(int n)
{
    vector<int32_t> matrix;
    for(int i=0;i<n*n;i++)
    {
        if(i/n == i%n) // diagonal check
            matrix.push_back(0);
        else
            matrix.push_back(rand()%2);
    }
    return matrix;
}

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

int get_index(int i, int j, int w)
{
    return i*w+j;
}

int main(int argc, char *argv[])
{
    if(argc != 4)
    {
        fprintf(stderr,"ERROR: Input should come in the form of two int values: # of blocks, # of slave nodes per block\n");
        exit(1);
    }
    srand(time(0));

    int block_total = atoi(argv[1]); // number of blocks of data
    int red_mult = atoi(argv[2]); // redundancy multiplier
    int vertex_total = atoi(argv[3]); // for testing
    int block_size = vertex_total/block_total; // ** NUMBER OF ROWS STOP FORGETTING **
    int slave_count = 0;

    vector<int32_t> solution(vertex_total*vertex_total);
    int sol_count = 0;

    int32_t hard[16] = {0,0,2,0, 4,0,3,0, 0,0,0,2, 0,1,0,0};
    vector<int32_t> adj_matrix(hard, hard+sizeof(hard)/sizeof(int32_t) ); //generate_adj_matrix(vertex_total);
    print_adj_matrix(adj_matrix, vertex_total);
    for(int a=0; a<vertex_total; a++)
        for(int b=0; b<vertex_total; b++)
        {
            if(a!=b)
            {
                if(adj_matrix[get_index(a,b,vertex_total)] == 0)
                    adj_matrix[get_index(a,b,vertex_total)]=1000;
            }
        }

    int opt = TRUE;
    // NOTE: client_socket can be used to map a IDs (as indices) to slave nodes.
    // IMPORTANT: Right now, we're using the indices of client_socket to serve as IDs.
    int master_socket , addrlen , new_socket , client_socket[block_total*red_mult+1] , max_clients = block_total*red_mult , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025];  //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;

    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(slave_count < block_total+1)
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++)
        {
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);

            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR))
        {
            printf("select error");
        }

        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //increment count of number of slaves we'll have to work with
            slave_count++;

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

            //send newly connected slave its data
            int size_sizes = 4; //haha
            int32_t sizes[size_sizes];
            sizes[0] = vertex_total;
            sizes[2] = slave_count-1; //setting ID. This will always work, but the harder part of redundancy is incrementing slave_count properly.
            sizes[3] = block_total;

            if(slave_count == vertex_total && vertex_total%block_total>0)
            {
                int send_size = vertex_total*(vertex_total%block_total)*sizeof(int32_t);
                sizes[1] = send_size;

                if( send(new_socket, sizes, sizeof(int32_t)*size_sizes, 0) != sizeof(int32_t)*size_sizes )
                {
                    perror("send");
                }
                if( send(new_socket, &adj_matrix[0]+(slave_count-1)*vertex_total*(vertex_total%block_total), send_size, 0) != send_size )
                {
                    perror("send");
                }
            }
            else {
                int send_size = vertex_total*(block_size)*sizeof(int32_t);
                sizes[1] = send_size;

                if( send(new_socket, sizes, sizeof(int32_t)*size_sizes, 0) != sizeof(int32_t)*size_sizes )
                {
                    perror("send");
                }
                if( send(new_socket, &adj_matrix[0]+(slave_count-1)*vertex_total*block_size, send_size, 0) != send_size )
                {
                    perror("send");
                }
            }

            printf("Data sent successfully to slave node %d\n",slave_count-1);

            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++)
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);

                    break;
                }
            }
        }
    }

    //clear the socket set
    FD_ZERO(&readfds);

    //add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    //add child sockets to set
    for ( i = 0 ; i < max_clients ; i++)
    {
        //socket descriptor
        sd = client_socket[i];

        //if valid socket descriptor then add to read list
        if(sd > 0)
            FD_SET( sd , &readfds);

        //highest file descriptor number, need it for the select function
        if(sd > max_sd)
            max_sd = sd;
    }

    vector<int32_t> row_k(vertex_total);
    for(int k=0;k<vertex_total;k++)
    {
        printf("k: %d\n",k);
        //printf("%d\n",k/(block_size));
        sd = client_socket[k/(block_size)];
        int size_req = 2;
        int32_t req[size_req];

        // Send command 0 (request)
        req[0] = 0;
        req[1] = k;

        if(FD_ISSET(sd, &readfds))
        {
            // Command 0 sent (request row)
            if( send(sd, &req, sizeof(int32_t)*size_req, 0) != sizeof(int32_t)*size_req )
            {
                perror("send");
            }
            //printf("Socket Descriptor: %d\n",sd);
            read(sd, &row_k[0],sizeof(int32_t)*vertex_total);
            //print_adj_matrix(row_k,vertex_total);
        }

        // Send out data
        for(int j=0;j<block_total*red_mult;j++)
        {
            printf("Sending row_k to slave ID: %d\n",j);
            sd = client_socket[j];
            if(FD_ISSET(sd, &readfds))
            {
                printf("Command %d: k=%d\n",req[0],req[1]);
                req[0] = 1;
                if( send(sd, &req, sizeof(int32_t), 0) != sizeof(int32_t) )
                {
                    perror("send");
                }
                printf("BROADCASTING ROW %d : ",k);
                print_adj_matrix(row_k,vertex_total);
                if( send(sd, &row_k[0],sizeof(int32_t)*vertex_total,0) != sizeof(int32_t)*vertex_total )
                {
                    perror("send");
                }

                //read(sd, &ack_id, sizeof(int32_t));
                //printf("%d %d",ack_id, cc);
            }
        }

        puts("Initializing boolean array");
        bool ack_arr[block_total];
        for(int j=0;j<block_total;j++)
        {
            ack_arr[j]=0;
        }

        int t = 0;
        int ack_id = 0;
        int ack_count = 0;
        while(ack_count < block_total)
        {
            puts ("Refreshing socket fd's");
            //clear the socket set
            FD_ZERO(&readfds);

            //add master socket to set
            FD_SET(master_socket, &readfds);
            max_sd = master_socket;

            //add child sockets to set
            for ( i = 0 ; i < max_clients ; i++)
            {
                //socket descriptor
                sd = client_socket[i];

                //if valid socket descriptor then add to read list
                if(sd > 0)
                    FD_SET( sd , &readfds);

                //highest file descriptor number, need it for the select function
                if(sd > max_sd)
                    max_sd = sd;
            }
            puts("Waiting for socket activity ... ");
            //wait for an activity on one of zthe sockets , timeout is NULL , so wait indefinitely
            activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);
            puts("Socket activity detected on ...");
            if ((activity < 0) && (errno!=EINTR))
            {
                printf("select error");
            }

            if (FD_ISSET(master_socket, &readfds)) { puts("MASTER_SOCKET"); continue; }

            for(int j=0;j<block_total*red_mult;j++)
            {
                sd = client_socket[j];
                if(ack_arr[j] == 0 && FD_ISSET(sd, &readfds))
                {
                    printf("SLAVE_SOCKET %d\n",j);
                    t = read(sd, &ack_id, sizeof(int32_t));
                    if(t>0)
                    {
                        printf("ACK ID:%d\n",ack_id);
                        ack_arr[ack_id]=1;
                        ack_count++;
                    }
                }
            }
        }

    }

    for(int k=0;k<vertex_total;k++)
    {
        sd = client_socket[k/(block_size)];
        int size_req = 2;
        int32_t req[size_req];

        // Send command 0 (request)
        req[0] = 0;
        req[1] = k;

        if(FD_ISSET(sd, &readfds))
        {
            // Command 0 sent (request row)
            if( send(sd, &req, sizeof(int32_t)*size_req, 0) != sizeof(int32_t)*size_req )
            {
                perror("send");
            }
            //printf("Socket Descriptor: %d\n",sd);
            read(sd, &row_k[0],sizeof(int32_t)*vertex_total);
            //print_adj_matrix(row_k,vertex_total);
            for(int c=0;c<row_k.size();c++)
            {
                solution[sol_count] = row_k[c];
                sol_count++;
            }
        }
    }

    for(i=0;i<max_clients;i++)
    {
        sd=client_socket[i];
        close(sd);
    }

    for(int x=0;x<solution.size();x++)
    {
        if(solution[x]==1000)
            solution[x]=0;
    }

    print_adj_matrix(solution, vertex_total);
    return 0;
}
