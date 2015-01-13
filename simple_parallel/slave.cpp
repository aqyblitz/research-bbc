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

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

using namespace std;

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int k = 0;

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

    /*printf("Please enter the message: ");

    bzero(buffer,256);
    fgets(buffer,255,stdin);

    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0)
         error("ERROR writing to socket");
    bzero(buffer,256);
    */

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
    int id = sizes[2];
    int block_total = sizes[3];

    for(int i=0; i<adj_block.size(); i++)
    {
        //(adj_block[i] == 0 )
        //    adj_block[i] = vertex_total+1; // unweighted graphs only, n+1
        if(i%vertex_total==0 && i!=0)
            puts("");
        printf("%d ",adj_block[i]);
    }
    puts("");

    while(true)
    {
        int size_req = 2;
        int req[size_req];
        puts("why won't you love me");
        n = read(sockfd,&req,size_req*sizeof(int32_t));
        printf("Bytes read:%d Command:%d\n",n,req[0]);
        k = req[1];
        if(req[0]==0)
        {
            int send_size = vertex_total*sizeof(int32_t);
            puts("Hello world");
            /*
             * vertex_total * row_index
             *                row_index = k % (rows in a block)
             *                                (rows in a block) = vertex_total/block_total
            */
            puts("hallooo");
            if(send(sockfd, &adj_block[0]+vertex_total*(k%(vertex_total/block_total)), send_size, 0) != send_size)
            {
                perror("send");
            }

        }
    }
/*
    for(int k=0;k<=vertex_total;k++)
    {
        if(k==id*sizes[1]/(sizeof(int32_t)*vertex_total))
        {

        }
        vector<int32_t> row_k(vertex_total);
        n = read(sockfd,&row_k[0],vertex_total*sizeof(int32_t));
        k +=1
    }
*/
    close(sockfd);

    //delete adj_block;
    return 0;
}
