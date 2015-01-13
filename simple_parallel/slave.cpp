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

    int32_t sizes[2];

    n = read(sockfd,sizes,2*sizeof(int32_t));
    if (n < 0)
         error("ERROR reading from socket");
    for(int i=0; i<2; i++)
        printf("%d ",sizes[i]);

    // size = # of vertices (elements per row) TIMES the number of rows = total length of vector
    vector<int32_t> adj_block(sizes[1]/sizeof(int32_t));
    //printf("%d",adj_block.size());
    n = read(sockfd,&adj_block[0],sizes[1]*sizeof(int32_t));
    if (n < 0)
         error("ERROR reading from socket");

    for(int i=0; i<adj_block.size(); i++)
    {
        if(i%sizes[0]==0 && i!=0)
            puts("\n");
        printf("%d ",adj_block[i]);
    }

    close(sockfd);

    //delete adj_block;
    return 0;
}
