/*
Command line arguments:
1) the name or address of the server
2) The port on which the server is running (uint8_t)

USING SOME CODE FROM PROGRAM 1
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

int serverSock = 0;


// usrNmProtocol communicates with the server to find a valid username
void usrNmProtocol (){
  int valid = 0;
  char prtptName [12];
  uint8_t lenPrtptNm = 0;
  int garbage = 'a';
  char response = 'a';

  // loop prompts user to enter usrname < 10 chars
  while (valid != 1){
    memset (prtptName, '\0', 12);
    printf("Please enter a username of a participant.\n");
    // make sure the entered name is 10 <= chars (repeat until successful)
    fgets (prtptName, 12, stdin);

    // only scan 10 chars and flush the rest of the buffer
    int tooBig = 0;
    if (prtptName[10] != '\0' && prtptName[10] != '\n'){
      tooBig = 1;
      ungetc ('a', stdin);
      while ((garbage = getchar()) != EOF && garbage != '\n'){
      }
    }

    // if the inputted username is 10 chars or less
    // send the username length then username to the server
    if (tooBig == 0){
      lenPrtptNm = (uint8_t) strnlen (prtptName, 12) - 1; // -1 because of \n char
      prtptName [lenPrtptNm] = '\0'; // erases the \n and end of name
      send (serverSock, &lenPrtptNm, 1, 0);
      send (serverSock, &prtptName, lenPrtptNm, 0);
      // server response: observer already affiliated = 'T',
      // no participant with that name = 'N',
      // go ahead! participant w/o observer = 'Y';
      recv (serverSock, &response, 1, 0);

      if (response == 'T'){
        printf("Participant %s already has an affiliated observer.\nServer disconnecting.\n", prtptName);
        exit (EXIT_SUCCESS);
      }
      else if (response == 'N'){
        printf("No participant %s is active in the chat room.\nServer disconnecting.\n", prtptName);
        exit (EXIT_SUCCESS);
      }
      else if (response == 'Y'){
        printf("Attaching to participant %s.\nYou will now recieve all public and private messages to %s.\n\n", prtptName, prtptName);
        valid = 1;
      }
    }
  }
}


// an endless loop printing out what the server sends the client.
void recvProtocol (){
  char msg [1000];
  uint16_t netMsgLen = 0;
  uint16_t hostMsgLen = 0;

  while (1){
    memset (msg, '\0', 1000);
    int t1 = recv (serverSock, &netMsgLen, sizeof(netMsgLen), 0);
    if (t1 <= 0){
      printf("Exiting.\n");
      exit (EXIT_SUCCESS);
    }
    hostMsgLen = ntohs(netMsgLen);
    recv (serverSock, &msg, hostMsgLen, 0);
    msg[hostMsgLen+1]= '\0';
    printf("%s\n", msg);
  }
}


// setupServerSock literally sets up the server socket descriptor
int setupServerSock (char * hostArg, char * portArg){
  struct hostent *ptrh; // pointer to a host table entry
  struct protoent *ptrp; // pointer to a protocol table entry
  struct sockaddr_in sad; // structure to hold an IP address
  uint16_t port; // protocol port number
  char *host; // pointer to host name

  memset ((char *) &sad, 0, sizeof(sad)); // clear sockaddr structure
  sad.sin_family = AF_INET; // set family to Internet

  port = atoi(portArg); // convert to binary
  if (port > 0) // test for legal value
  	sad.sin_port = htons ((u_short) port);
  else {
  	fprintf (stderr, "Error: bad port number %s\n", portArg);
  	exit (EXIT_FAILURE);
  }

  host = hostArg; // if host argument specified

  // Convert host name to equivalent IP address and copy to sad.
  ptrh = gethostbyname(host);
  if ( ptrh == NULL ) {
  	fprintf(stderr,"Error: Invalid host: %s\n", host);
  	exit(EXIT_FAILURE);
  }

  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

  // Map TCP transport protocol name to protocol number.
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
  	fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
  	exit(EXIT_FAILURE);
  }

  // Create a socket.
  serverSock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (serverSock < 0) {
  	fprintf(stderr, "Error: Socket creation failed\n");
  	exit(EXIT_FAILURE);
  }

  // Connect the socket to the specified server.
  if (connect(serverSock, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
  	fprintf(stderr,"connect failed\n");
  	exit(EXIT_FAILURE);
  }
}


// main calls methods to establish the server socket
int main (int argc, char **argv){

  // check for correct number of args
  if (argc != 3){
    fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

  setupServerSock (argv[1], argv[2]);

  char cxnOK = 'O';
  recv (serverSock, &cxnOK, 1, 0);
  if (cxnOK == 'N'){
    printf("No space for another observer. Disconnecting from server.\n");
    printf("error in connection: %s\n", strerror(errno));
    close (serverSock);
    exit (EXIT_FAILURE);
  }

  usrNmProtocol ();

  recvProtocol ();


}
