/* A simple server in the internet domain using TCP
The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "crcgen.h"

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    // sockfd / newsockfd are file descriptors (array aubscripts to file descriptor table.
    // They store the values returned by the socket system call and the accept system call.
    // Portno is the port on which this server accepts connections
    int sockfd, newsockfd, portno;

    // Stores the size of the address of the client (needed for accept system call).
    socklen_t clilen;

    // Buffer.
    char buffer[256];

    // These structs contain "internet addresses." Defined in netinit/in.h
    // For some reason they all have some short sin_family that has to be AF_INET.
    // And sin_zero which is an array of 8 1-byte 0s...?
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    // Command line arguments.
    if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
    }

    // socket is inistialized (finally?). It's an int...
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // Possible address domains: AF_INET (Internet, as opposed to common file system)
    // Type of socket: SOCK_STREAM vs SOCK_DGRAM (continuous stream as if from a file or pipe or chunked messages
    // protocol: 0 (should usually be this); TCP for stream, UDP for datagram

    if (sockfd < 0)
    error("ERROR opening socket");

    // Sets all the values in the c string serv_addr to 0.
    bzero((char *) &serv_addr, sizeof(serv_addr));

    // The port number on which the server listens for connections is passed in.
    // atoi converts string of digits to int.
    portno = atoi(argv[1]);

    // serv_addr is the struct discussed above.
    // sin_family contains a code for the address family, should always be AF_INET
    serv_addr.sin_family = AF_INET;
    // sin_addr.s_addr: struct that only has some unsigned long s_addr. Contains the host IP address. INADDR_ANY gest the IP address of the machine the server is running on (neat!)
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    // This is some unsigned short that contains the port number. We have to convert it to network byte order using htons().
    serv_addr.sin_port = htons(portno);

    // The bind() system call binds the socket to an address. Takes the socket file descriptor, the bound address, and the size of the address.
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
        sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // System call allows the process to listen on the socket for connections. First arg is socket file descriptor, second is size of backlog queue.
    // Apparently shoudl be set to 5, the maximum size permitted by most systems.
    listen(sockfd,5);

    // The accept() call calls the process to block until a client connects to the server (ah-hah!).
    // Returns a new file descriptor; all communication on that connection should be done using that fd.
    // socket fd, reference pointer to address of the client on other end, size of the struc.
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd,
           (struct sockaddr *) &cli_addr,
           &clilen);
    if (newsockfd < 0)
    error("ERROR on accept");

    // Zero/initiazlie the buffer, read from the socket.
    // read() will block until there is something for it to read in the socket (i.e. after the client has executed a write()).
    bzero(buffer,256);
    // Return number of bytes read.
    int32_t* msg_info = new int32_t[5]();
    n = read(newsockfd,msg_info,5*sizeof(int32_t));

    int32_t* msg_info_crc_comp = new int32_t[4]();
    for(int i=0; i<4; i++) {
        msg_info_crc_comp[i] = ntohl(msg_info[i]);
        printf("%d ",ntohl(msg_info[i]));
        //printf("%d ",msg_info[i]);
    }
    printf("Received CRC: 0x%X\n",ntohl(msg_info[4]));
    printf("Received CRC: 0x%X\n",msg_info[4]);

    unsigned char* temp = reinterpret_cast<unsigned char*>(msg_info_crc_comp);
    crcgen gen = crcgen();
    uint32_t crc_val = gen.crc_compute(temp, sizeof(temp),ntohl(msg_info[4]));
    printf("Computed CRC: 0x%X\n",crc_val);

    if (n < 0) error("ERROR reading from socket");
    // Returns # of bytes written.
    n = write(newsockfd,"Message received.",17);
    if (n < 0) error("ERROR writing to socket");
    close(newsockfd);
    close(sockfd);

    delete msg_info;
    return 0;
}
