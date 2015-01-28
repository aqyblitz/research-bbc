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
 * TODO: Pipe to logs.
 * Logs should be adapted to become more verbose; I'll try to match certain "states" to the diagram.
 * It should be noted that clearing the socket file descriptor table constantly is probably a bad idea. It's either a miracle that my stuff works or the guys over at MITRE don't know exactly what's going on right now.
 * For now, it's been suggested that I migrate the clearing code to outside of the large loops. The issue that's generated is that IF some client attempts to connect to this server while the logs are being cleared (or before?), then the connection is lost. Should look a little more into this.
 * Should be noted that on k=1 (reproducible bug running on this environment) results in a failed connection to slave ID 1 on a 2-block, 4-vertex, 1-redundancy multiplier input. This should be fairly important to diagnose.
 * Diagnose the "extra slave node" problem. Honestly shouldn't be there.
 */
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
                // Infinitizing here.
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
    //puts("Waiting for connections ...");

    //clear the socket set
    FD_ZERO(&readfds);

    //add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    while(slave_count < block_total)
    {
        puts("STATE 0 : WAITING FOR CONNECTIONS ... ");
        /*//add child sockets to set
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
        }*/

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

            puts("STATE 1 : ACCEPTING CONNECTION ...");
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
                puts ("** NON-EVEN CASE (unique end case)");
                int send_size = vertex_total*(vertex_total%block_total)*sizeof(int32_t);
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
            else {
                puts ("** EVEN CASE");
                int send_size = vertex_total*(block_size)*sizeof(int32_t);
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
            for (i = 0; i < max_clients; i++)
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    //socket descriptor
                    client_socket[i] = new_socket;

                    //if valid socket descriptor then add to read list
                    if(client_socket[i] > 0)
                        FD_SET( client_socket[i] , &readfds);

                    //highest file descriptor number, need it for the select function
                    if(client_socket[i] > max_sd)
                        max_sd = client_socket[i];
                    printf("Adding to list of sockets as %d\n" , i);

                    break;
                }
            }
        }
    }

    vector<int32_t> row_k(vertex_total);
    puts("Beginning main iterative loop ... ");
    for(int k=0;k<vertex_total;k++)
    {
        printf("STATE 2 : Retrieve row k=%d\n",k);
        //printf("%d\n",k/(block_size));
        sd = client_socket[k/(block_size)];
        int size_req = 2;
        int32_t req[size_req];

        // Send command 0 (request)
        req[0] = 0;
        req[1] = k;

        int sock_code = FD_ISSET(sd, &readfds);
        if(sock_code)
        {
            // Command 0 sent (request row)
            printf("** Command %d: k=%d\n",req[0],req[1]);
            printf("** Requesting row %d from slave ID=%d",k,k/block_size);
            if( sock_code=send(sd, &req, sizeof(int32_t)*size_req, 0) != sizeof(int32_t)*size_req )
            {
                perror("send");
            }
            puts("** Command 0 sent ...");
            //printf("Socket Descriptor: %d\n",sd);
            read(sd, &row_k[0],sizeof(int32_t)*vertex_total);
            puts("STATE 3 : Row received");
            //DEBUG
            print_adj_matrix(row_k,vertex_total);
        }
        else
        {
            printf("-- ERROR: Socket ID %d not valid connection | %d\n",k/block_size,sock_code);
        }
    }
/*
        // Send out data
        for(int j=0;j<block_total*red_mult;j++)
        {
            puts("STATE 4 : Sending row to slave nodes");
            printf("** Command %d: k=%d\n",req[0],req[1]);
            printf("** Sending row_k to slave ID: %d\n",j);
            sd = client_socket[j];
            int sock_code = FD_ISSET(sd, &readfds);

            if(sock_code)
            {
                req[0] = 1;
                if( send(sd, &req, sizeof(int32_t)*size_req, 0) != sizeof(int32_t) )
                {
                    perror("send");
                }
                puts("** Command 1 sent ...");

                printf("** Sending row %d to slave ID=%d",k,j);
                print_adj_matrix(row_k,vertex_total);
                if( send(sd, &row_k[0],sizeof(int32_t)*vertex_total,0) != sizeof(int32_t)*vertex_total )
                {
                    perror("send");
                }
                printf("** Row %d sent to slave ID=%d\n",k,j);

                //read(sd, &ack_id, sizeof(int32_t));
                //printf("%d %d",ack_id, cc);
            }
            else {
				printf("-- ERROR: Socket ID %d not valid connection | %d\n",j,sock_code);
			}
        }
        */
        while(true) {}
/*
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
            puts("STATE 5: Waiting for socket activity (counting ACK) ... ");
            //wait for an activity on one of zthe sockets , timeout is NULL , so wait indefinitely
            activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);
            puts("** Socket activity detected on ...");
            if ((activity < 0) && (errno!=EINTR))
            {
                printf("select error");
            }

            if (FD_ISSET(master_socket, &readfds)) { puts("** SOURCE: MASTER_SOCKET"); continue; }

            for(int j=0;j<block_total*red_mult;j++)
            {
                sd = client_socket[j];
                if(ack_arr[j] == 0 && FD_ISSET(sd, &readfds))
                {
                    printf("** SOURCE: SLAVE ID=%d\n",j);
                    t = read(sd, &ack_id, sizeof(int32_t));
                    if(t>0)
                    {
                        printf("** ACK ID:%d\n",ack_id);
                        ack_arr[ack_id]=1;
                        ack_count++;
                    }
                }
            }
        }

    }*/
/*
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
				puts("Requesting processed data from slave nodes ...");
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

	 puts("Deinfinitizing the solution ...");
    for(int x=0;x<solution.size();x++)
    {
        if(solution[x]==1000)
            solution[x]=0;
    }

    print_adj_matrix(solution, vertex_total);
    */
    return 0;
}
