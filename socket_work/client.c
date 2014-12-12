#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } b_int = {0x01020304};

    return b_int.c[0] == 1; 
}

int main(int argc, char *argv[])
{
	int sockfd, portno, n;
   struct sockaddr_in serv_addr;
	
	// What is this? Struct defined in netdb.h that *DEFINES A HOST COMPUTER ON THE INTERNET*
	// char	*h_name			Official name of the host
	// char	**h_aliases		Zero-termianted array of alternate names for the host
	// int	h_addrtype		The type of address being returned (always AF_INET)
	// int	h_length			Length of bytes of the address
	//	char	**h_addr_list	Pointer to a list of network addresses for the named host. Returned in network byte order (big endian?)
	//		h_addr is an alias for the first address in the array of network addresses.
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
       fprintf(stderr,"ERROR, no such host\n"); // this if statement should make sense
       exit(0);
   }

	// Set values in serv_addr
   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
	// Because (host address) h_addr is a character string, we need to pull server->h_length bytes starting from where h_addr is; copy it into s_addr
   bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
	// Put in network byte order.
   serv_addr.sin_port = htons(portno);

	// connect() establishes a connection to the server.
	// Takes socket file descriptor, the address of host we wanna connect to & port number, size of the address.
   if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
       error("ERROR connecting");
	// Modify here.
	// Transfer:
	//	dataTypeSize			sizeof(int)
	// numDimensions			2 is implied
	// node_id					id of node's block
	//	d1 size					# of rows node is getting
	// d2 size					# of vertices in the graph
	// rows						parts of the adj matrix
	// CRC						?

	// First message: Send data_type_size, node_id, d1, and d2. 4 ints...?
	int32_t msg_info[] = {htonl(sizeof(int)), htonl(0), htonl(4), htonl(6)};
   //printf("Please enter the message: ");
   bzero(buffer,256);
	// This reads data from stdin.
   //fgets(buffer,255,stdin);
	//int32_t my_32_
   n = write(sockfd, (char*) msg_info, sizeof(msg_info));

   if (n < 0) 
        error("ERROR writing to socket");
   bzero(buffer,256);
   n = read(sockfd,buffer,255);
   if (n < 0) 
        error("ERROR reading from socket");
   printf("%s\n",buffer);
   close(sockfd);
   return 0;
}
