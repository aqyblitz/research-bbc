#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

void error(const char *msg)
{
    perror(msg);
    exit(0);
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

void relax(vector<int32_t> &adj_block, int id, vector<int32_t> row_k, int k, int b_s, int v_t)
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
    if(k==1)
    {puts("***********************");
        print_adj_matrix(adj_block,v_t);
        puts("------------------------------");
        }
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int32_t k = 0;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    int size_sizes = 4;
    int32_t sizes[size_sizes];

    n = read(sockfd,sizes,size_sizes*sizeof(int32_t));
    if (n < 0)
         error("ERROR reading from socket");
    for(int i=0; i<size_sizes; i++)
        printf("%d ",sizes[i]);
    puts("");

    // size = # of vertices (elements per row) TIMES the number of rows = total length of vector
    vector<int32_t> adj_block(sizes[1]/sizeof(int32_t));
    //printf("%d",adj_block.size());
    n = read(sockfd,&adj_block[0],sizes[1]*sizeof(int32_t));
    if (n < 0)
         error("ERROR reading from socket");
    int vertex_total = sizes[0];
    int32_t id = sizes[2];
    int block_total = sizes[3];

    for(int i=0; i<adj_block.size(); i++)
    {
        if(i%vertex_total==0 && i!=0)
            puts("");
        printf("%d ",adj_block[i]);
    }
    puts("");

    vector<int32_t> row_k(vertex_total);
    while(true)
    {
        int32_t size_req = 2;
        int32_t req[size_req];
        puts("Waiting for command ...");
        n = read(sockfd,&req,size_req*sizeof(int32_t));
        printf("Bytes read:%d Command:%d k:%d\n",n,req[0],req[1]);
        k = req[1];

        if(req[0]==0)
        {
            int send_size = vertex_total*sizeof(int32_t);

            /*
             * vertex_total * row_index
             *                row_index = k % (rows in a block)
             *                                (rows in a block) = vertex_total/block_total
            */
            if(send(sockfd, &adj_block[0]+vertex_total*(k%(vertex_total/block_total)), send_size, 0) != send_size)
            {
                perror("send");
            }
        }
        if(req[0]==1)
        {
            printf("** K VALUE : %d\n",k);
            read(sockfd,&row_k[0],vertex_total*sizeof(int32_t));
            // Call relaxation function.
            int b_s=0;
            if(id==block_total-1)
                b_s=vertex_total/block_total;
            else
                b_s=vertex_total%block_total;
            printf("%d\n !!!!!!!!!!!!!!!!!1 \n",k);
            print_adj_matrix(row_k,vertex_total);
            relax(adj_block,id,row_k,k,b_s,vertex_total);
            // Send back "Done"
            print_adj_matrix(adj_block,vertex_total);
            if( send(sockfd,&id,sizeof(int32_t),0) != sizeof(int32_t))
            {
                perror("send");
            }

            printf("SENT ACK %d\n",id);
        }
    }

    close(sockfd);

    return 0;
}
