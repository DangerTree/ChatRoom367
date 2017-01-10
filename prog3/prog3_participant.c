/*
Participant client to communicate with prog3 server.
This participant will be able send messages to the server.

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

#define MAX_MSG 1000

int serverSock = 0;

void messageProtocol (){
  char newMsg[MAX_MSG];
  uint16_t msgLen = 0;
  while (1){
    memset (newMsg, '\0', MAX_MSG);
    // prompt user for imput
    printf("Enter message: ");
    fgets (newMsg, MAX_MSG, stdin);
    msgLen = strnlen (newMsg, MAX_MSG);
    if (msgLen <= 1000){
      uint16_t netMsgLen = htons (msgLen);
      send (serverSock, &netMsgLen, sizeof(netMsgLen), 0);
      send (serverSock, &newMsg, msgLen, 0);
    }
    else {
      printf("Message is too long. Please enter <= 1000 chars.\n");
    }
  }
}


// usrNmProtocol communicates with the server to find a valid username
void usrNmProtocol (){
  int valid = -1;
  char usrNm [12];
  uint8_t lenUsrNm = 0;
  char garbage = 'a';
  char response = 'a';

  // a loop asking the user to enter a username < 10 chars w/o special chars or " ". Loop ends when input is good.
  while (valid != 1){

    memset (usrNm, '\0', 12);
    printf("Please enter a username of 10 or fewer characters.\nNo special chars or spaces.\n");
    // make sure the entered name is 10 <= chars (repeat until successful)
    fgets (usrNm, 12, stdin); // fills in 11 bytes, then a \0

    // if user input more chars than 10, null out the stdin buffer and repeat
    int tooBig = 0;
    if (usrNm[10] != '\0' && usrNm[10] != '\n'){
      tooBig = 1;
      ungetc ('a', stdin);
      while ((garbage = getchar()) != EOF && garbage != '\n'){
      }
    }

    // if the guess is a good length
    if (tooBig == 0){
      lenUsrNm = (uint8_t) strnlen(usrNm, 12) - 1; // -1 because of \n char

      // Make sure all chars are valid
      int charsValid = 1;
      char oneChar = 'a';
      for (uint16_t i = 0; i < lenUsrNm; i++){
        oneChar = usrNm[i];
        if ( !( (oneChar >= '0' && oneChar <= '9') || (oneChar >= 'A' && oneChar <= 'Z') || (oneChar >= 'a' && oneChar <= 'z') || oneChar == '_') ){
          charsValid = 0;
        }
      }
      usrNm [lenUsrNm] = '\0';

      // send good username to server
      if (charsValid == 1){
        // once client has input a correct guess, send it to the server to see if its ok
        send (serverSock, &lenUsrNm, 1, 0); // length of guess
        send (serverSock, &usrNm, lenUsrNm, 0); // send guess
        // receive server feedback: Y = good, T = taken (guess again + timer reset), I = invalid username
        recv (serverSock, &response, 1, 0);

        if (response == 'I'){
          printf("server response: %c Invalid username!\n", response);
        }
        else if (response == 'T'){
          printf ("That user name is taken. Please select another.\n");
        }
        else {
          valid = 1;
        }
      }
    }
  }
}



int setupServerSock (char * hostArg, char * portArg, int serverSock){
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
  return serverSock;
}


int main (int argc, char **argv){

  // check for correct number of args
  if (argc != 3){
    fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

  serverSock = setupServerSock (argv[1], argv[2], serverSock);

  // check if connection is OK (participant limit hasn't been reached)
  char cxnOK = 'O';
  recv (serverSock, &cxnOK, 1, 0);
  if (cxnOK == 'N'){
    printf("error in connection: %s\n", strerror(errno));
    close (serverSock);
    exit (EXIT_FAILURE);
  }
  usrNmProtocol ();

  messageProtocol ();

}
