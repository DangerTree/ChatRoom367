/*
Prog3 Server: essentially a chat room,
this server relays messages between participants, who write but cannot see messages,
and observers, who can affiliate with a participant to read messages but not write.

Commandline arguments:
the port on which the server listens for participants (uint8_t)
the port on which the server listens for observers (uint8_t)

USING SOME CODE FROM PROGRAM 1

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_CLIENT_NUM 255
#define TIMER_MAX 60

typedef enum bool{
  false, true
} bool;

typedef struct {
  char pName[11]; // the username for a participant or affilitated participant's username for an observer
  char type; // 'O' if Node is an observer, 'P' for a participant
  int sockDes;
  int obsvSock; // the observed socket descriptor if this is a participant
  int prtSockIdx; // location in array of affilitated participant node if this is an observer
  long timer; // how much time left before socket disconnected. Field only used when negotiating username.left before disconnect and socket disconnected
  int state; // 0 = just connected, 1 == needs to establish username, 3 = connection established
} Node;


Node* pSockArray = NULL;
Node* oSockArray = NULL;
Node** timedSocks = NULL; // an array of pointers to nodes in the pSockArray and oSockArray arrays that are under a timer
Node * lowestTimedNode = NULL;
int pNum = 0;
int oNum = 0;
int tNum = 0;
int participant_sock = 0; // listening sock for new participants
int observer_sock = 0; // listening sock for new observers
char Y = 'Y';
char N = 'N';

// endNode essentially nulls out all fields of a client struct
void endNode (Node * activeNode){
  printf("Closing socket %s in endNode\n", activeNode->pName);
  close (activeNode->sockDes);
  memset (activeNode->pName, '\0', 11);
  activeNode->sockDes = -1;
  activeNode->state = 0;
  activeNode->timer = 0;
  activeNode->obsvSock = 0;
  if (activeNode->type == 'P'){
    pNum--;
  }
  else {
    oNum--;
  }
}


// takes an address to a node which is in oSockArray or pSockArray
// and removes it from the list of addresses in timedSocks
void removeTimer (Node * node){
  for (int k = 0; k < tNum; k++){
    if (timedSocks[k] == node){
      timedSocks[k] = NULL;
    }
  }
  tNum--;
}


// updates lowestTimedNode with the next expiring node
void findLowestTimer (){
  int lowestTimer = TIMER_MAX;
  int lowestIdx = -1;
  // find the index of the lowest timed node
  for (int i = 0; i < tNum; i++){
    if (timedSocks[i]->timer <= lowestTimer){
      lowestTimer = timedSocks[i]->timer;
      lowestIdx = i;
    }
  }
  lowestTimedNode = timedSocks[lowestIdx];

}


// updates (by subtracting time off) the # of seconds of live timers in the timedSocks array
void refreshTimers (int elapsedTime){

  if (tNum != 0){ // if there are any more nodes with active timers
    // update all timers
    for (int k = 0; k < tNum; k++){
      int oldTime = timedSocks[k]->timer;
      timedSocks[k]->timer = oldTime - elapsedTime;
    }
    findLowestTimer();
  }

  else {
    lowestTimedNode = NULL;
  }
}


// sends a given msg of lenMsg bytes to all observers.
void sendAll (char * msg, uint16_t lenMsg){

  uint16_t toSendLen = htons (lenMsg);

  for (int j = 0; j < oNum; j++){
    send (oSockArray[j].sockDes, &toSendLen, sizeof(toSendLen), 0);
    send (oSockArray[j].sockDes, msg, lenMsg, 0);
  }

}



/* receives a message from a participant socket parses it into the best format
relays private messages or calls sendAll to relay message to all observers
RETURNS: 0 if socket is dead
RETURNS: num of bytes recv'd if socket is live
*/
int processPtcptMsg (Node * activeNode){
  uint16_t msgLen = 0; // recieve msg length
  int usrNmLen = 0;

  int t1 = 1;
  if (t1 = (recv (activeNode->sockDes, &msgLen, 2, 0)) == -1){
    //printf("Message length retreival error %s\n", strerror(errno));
    if (t1 == 0 || msgLen > 1000){
      return 0;
    }
  }
  msgLen = ntohs (msgLen);
  char newMsg[msgLen + 1];
  int t2 = recv (activeNode->sockDes, &newMsg, msgLen, 0);
  if (t2 <= 0){
    return 0;
  }
  newMsg[msgLen-1] = '\0';

  // check if it is a private message
  if (newMsg[0] == '@'){
    char destUsr [11];
    char * restMsg;
    int destSock = 0;
    for (int k = 0; k < 10; k++){ // ignores the '@' char
      if (newMsg[k] == ' '){
        strncpy (destUsr, newMsg+1, k-1); // skip over '@'
        destUsr[k-1] = '\0';
        usrNmLen = k - 1;
        restMsg = newMsg + k + 1; // the message portion
        // find that participant
        for (int i = 0; i < pNum; i++){
          // if there is an active participant of that name find the socket
          if (strncmp (pSockArray[i].pName, destUsr, k) == 0){
            if (pSockArray[i].obsvSock == 0){
              return 1;
            }
            destSock = pSockArray[i].obsvSock;
          }
        }
      }
    }

    // if there was no participant of that name, send warning to sender
    if (destSock == 0){
      uint16_t errorMsgLen = usrNmLen + 31; // 31 in num of chars in  "Warning: [...]" msg
      char errorMsg [errorMsgLen];
      snprintf (errorMsg, errorMsgLen, "Warning: user %s doesn't exist...", destUsr);
      //printf("message to send: [%s]\n", errorMsg);
      uint16_t netMsgLen = htons (errorMsgLen);
      if (activeNode->obsvSock != 0){
        send (activeNode->obsvSock, &netMsgLen, sizeof(errorMsgLen), 0);
        send (activeNode->obsvSock, errorMsg, errorMsgLen, 0);
      }
      return t2;
    }

    else {
      uint16_t totalMsgLen = msgLen + 14;
      int paddingNum = 10 - strnlen(activeNode->pName, 11);
      char finalMsg[totalMsgLen];
      sprintf (finalMsg, "*%*s%s: %s", paddingNum, " ", activeNode->pName, restMsg);
      //printf("finalMsg after formatting:\n[%s]\n", finalMsg);

      uint16_t netTotalMsgLen = htons (totalMsgLen);
      send (destSock, &netTotalMsgLen, sizeof(netTotalMsgLen), 0);
      send (destSock, finalMsg, totalMsgLen, 0);

      return t2;
    }
  }
  int paddingNum = 10 - strnlen(activeNode->pName, 11);
  char finalMsg[msgLen + 14];
  sprintf (finalMsg, ">%*s%s: %s", paddingNum, " ", activeNode->pName, newMsg);
  printf("%s\n", finalMsg);
  sendAll (finalMsg, (uint16_t) msgLen + 14);

  return t2;
}



/* receives a len of message then message consisting of username from participant
if username has valid chars and isn't taken,
  function updates node structure, calls sendAll and removes node from timer array
if username has valid chars but is taken
  function informs participant and resets timer
if username has invalid chars, connection to participant is closed.
RETURNS: 1, if a username was valid and found in time
RETURNS: 0 is socket closed or time out occurred
*/
int makePcptUserName (Node * activeNode){

  uint8_t lenUserName = 0;
  char T = 'T';
  char I = 'I';
  bool isValid = false;
  char userName[11];

  // receive username
  int t1 = recv (activeNode->sockDes, &lenUserName, 1, 0);
  int t2 = recv (activeNode->sockDes, &userName, lenUserName, 0);
  if (t1 == 0 || t2 == 0){
    return 0; // SOCKET HAS CLOSED
  }
  userName[lenUserName] = '\0';

  // check if username chars are valid (only letters, numbers, and _)
  bool validChars = true;
  for (uint16_t i = 0; i < lenUserName; i++){
    char oneChar = userName[i];
    if ( !( (oneChar >= '0' && oneChar <= '9') || (oneChar >= 'A' && oneChar <= 'Z') || (oneChar >= 'a' && oneChar <= 'z') || oneChar == '_') ){
      send (activeNode->sockDes, &I, 1, 0);
      validChars = false;
      //printf("userName %s consists of invalid chars!\n", userName);
    }
  }

  // check if name is already taken
  bool nameFree = true;
  if (validChars == true){
    for (int i = 0; i < pNum; i++){
      if (strncmp (pSockArray[i].pName, userName, 10) == 0){
        send (activeNode->sockDes, &T, 1, 0);
        nameFree = false;
        activeNode->timer = TIMER_MAX;
        findLowestTimer ();
        return 1;
      }
    }
  }
  // FOUND A GOOD USERNAME
  if (validChars == true && nameFree == true){
    send (activeNode->sockDes, &Y, 1, 0);
    isValid = true;
    strncpy (activeNode->pName, userName, 11);
    activeNode->state = 2; // now this participant is ready to exchange messages
    char announceP [32];
    snprintf (announceP, 32, "User %s has joined.", userName);
    sendAll (announceP, (uint16_t) strnlen(announceP, 32));
    removeTimer (activeNode); // take out of timer list
    return 1;
  }
  return 0;
}



/* fills a participant node and adds it to the pSockArray structure and timer array
RETURNS: 0 if max number of participants reached
RETURNS: 1 upon success
*/
int newParticipant (){
  int newPSock = 0;
  struct sockaddr_in pSockArraytruct;
  unsigned int alen = sizeof (struct sockaddr_in);

  // bind a sockDes to a the new client
  if ( (newPSock = accept (participant_sock, (struct sockaddr *) &newPSock, &alen)) < 0){
    fprintf (stderr, "Error: Accept failed\n");
    exit (EXIT_FAILURE);
  }
  // if there are already 255 participants, don't allow another + close the socket
  if (pNum >= 255){
    send (newPSock, &N, 1, 0);
    close (newPSock);
    return 0;
  }
  // if there is space in the chat room, tell the client and add a Node
  else {
    send(newPSock, &Y, 1, 0);
    pSockArray[pNum].sockDes = newPSock;
    pSockArray[pNum].type = 'P';
    pSockArray[pNum].state = 1; // indicating that username still needs to be setup
    pSockArray[pNum].timer = TIMER_MAX;
    pSockArray[pNum].obsvSock = 0;
    timedSocks [tNum] = &pSockArray[pNum]; // add this socket to array of timed socks
    if (tNum == 0){ // if this is only timed thing
      lowestTimedNode = &pSockArray[pNum]; // assign it to the note with lowest timer
    }
    printf("New participant using socket descriptor: %d\n", pSockArray[pNum].sockDes);
    tNum++;
    pNum++;
  }
  return newPSock;
}


/* fills an observer node and adds it to the oSockArray structure and timer array
RETURNS: 0 if max number of observers reached
RETURNS: 1 upon success
*/
int newObserver (){
  int newOSock = 0;
  struct sockaddr_in oSockArraytruct;
  unsigned int alen = sizeof (struct sockaddr_in);
  // bind a sockDes to a the new client
  if ( (newOSock = accept (observer_sock, (struct sockaddr *) &newOSock, &alen)) < 0){
    fprintf (stderr, "Error: Accept failed\n");
    exit (EXIT_FAILURE);
  }
  // if there are already 255 observers, don't allow another
  if (oNum >= 255){
    send (newOSock, &N, 1, 0);
    close (newOSock);
    return 0;
  }
  // otherwise, fill the next observer struct and increment number of observers
  else {
    send (newOSock, &Y, 1, 0);
    oSockArray[oNum].sockDes = newOSock;
    oSockArray[oNum].type = 'O';
    oSockArray[oNum].state = 1; // affiliation with participant still needs to be established
    oSockArray[oNum].timer = TIMER_MAX;
    timedSocks [tNum] = &oSockArray[oNum]; // add this socket to array of timed socks
    if (tNum == 0){ // if this is only timed thing
      lowestTimedNode = &oSockArray[oNum]; // assign it to the note with lowest timer
    }
    printf("New observer using socket descriptor %d\n", newOSock);
    tNum++;
    oNum++;
  }
  return newOSock;
}


/* affiliate an observer with a participant
RETURNS: 0 if observer quits or its specified participant doesn't exist or already has an observer
RETURNS: 1 upon success
*/
int affiliateObserver (Node* activeONode){
  char Y = 'Y';
  char N = 'N';
  char T = 'T';

  uint8_t lenPName = 0;
  bool isValid = false;
  char pNameGuess[11];

  // receive username length
  int t1 = recv (activeONode->sockDes, &lenPName, 1, 0);

  // receive the observer's participant to affiliate with's name
  int t2 = recv (activeONode->sockDes, &pNameGuess, lenPName, 0);
  if (t1 <= 0 || t2 <= 0){
    return 0; // SOCKET HAS CLOSED
  }
  pNameGuess[lenPName] = '\0';

  // check if this name is an active participant
  for (int k = 0; k < pNum; k++){
    // if a participant of that name exists in the server
    if (strncmp (pSockArray[k].pName, pNameGuess, 10) == 0){
      // if that prtcpt does NOT already have an observer
      if (pSockArray[k].obsvSock == 0){
        activeONode->state = 2;
        pSockArray[k].obsvSock = activeONode->sockDes;
        activeONode->prtSockIdx = k;
        send (activeONode->sockDes, &Y, 1, 0);
        // and tell every one that a new observer has joined
        char * announceO= "A new observer has joined.";
        sendAll (announceO, (uint16_t) strnlen (announceO, 32));
        removeTimer (activeONode); // remove node from array of nodes with active timers
        return 1;
      }
      // if the prtcpt DOES already have an observer
      else {
        send (activeONode->sockDes, &T, 1, 0);
        removeTimer (activeONode);
        return 0; // indicates that observer should erased from structure
      }
    }
  }
  // if that participant does NOT exist in the server at all
  send (activeONode->sockDes, &N, 1, 0);
  removeTimer (activeONode);
  return 0; // indicates that observer should erased from structure
}


int observerChange (Node* activeONode){
  return 0;
}


// initializes the listening socket
// returns the socket descriptor
int setupListenSock (char * portArg, int sock){
  struct protoent *ptrp; // pointer to a protocol table entry
  struct sockaddr_in sad; // structure to hold server's address
  uint16_t port; /* protocol port number */
  int optval = 1; /* boolean value when we set socket option */

  // set up the socket address structures
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */
  sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

  // attempt to associate the given port number in the socket address structure
  port = atoi(portArg); // convert argument to binary
  if (port > 0) { // test for illegal value
    sad.sin_port = htons((u_short)port);
  } else { // print error message and exit
    fprintf (stderr,"Error: Bad port number %s\n", portArg);
    exit (EXIT_FAILURE);
  }

  // Map TCP transport protocol name to protocol number
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  // Create a socket
  sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sock < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  // Allow reuse of port - avoid "Bind failed" issues
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }

  // Bind a socket structure (local address) to the socket
  if (bind(sock, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }

  // Start listening on socket and specify size of request queue
  if (listen(sock, 510) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }
  return sock;
}



int main(int argc, char **argv) {

  int nfds = 0; // largest socket descriptor
  int result = 0; // result of select
  fd_set readset;
  fd_set masterset;

  // load arguments into appropriate variables
	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);//newParticipant
	}
  // initiate fd_sets
  FD_ZERO (&readset);
  FD_ZERO (&masterset);
  // initiate participant and observer ports
  participant_sock = setupListenSock (argv[1], participant_sock); // initialize participant listening socket w/ port argument
  observer_sock = setupListenSock (argv[2], observer_sock); // initialize participant listening socket w/ port argument
  // add them to the masterset
  FD_SET (participant_sock, &masterset);
  FD_SET (observer_sock, &masterset);

  // set up arrays of participants and observers
  pSockArray = calloc (MAX_CLIENT_NUM, sizeof (Node));
  oSockArray = calloc (MAX_CLIENT_NUM, sizeof (Node));
  // set up array of pointers to stuff in pSockArray and oSockArray which are timed
  timedSocks = calloc (MAX_CLIENT_NUM * 2, sizeof (Node *));
  lowestTimedNode = calloc (1, sizeof (Node));

  // set nfds to largest socket
  nfds = (participant_sock > observer_sock) ? participant_sock : observer_sock;

	// Main server loop - accept and handle requests
	while (1) {

    // copy the masterset to the readset
    memcpy (&readset, &masterset, sizeof(masterset));

    // call select on readset to see which sockets are ready to read
    struct timeval timev;
    if (tNum != 0){ // if there are timed things
      timev.tv_sec = lowestTimedNode->timer;
      result = select (nfds+1, &readset, NULL, NULL, &timev);
    }
    else {
      result = select (nfds+1, &readset, NULL, NULL, NULL);
    }
    if (result == -1){
      printf("Select error: %s\n", strerror (errno));
      exit (EXIT_FAILURE);
    }

    // if a timeout happened
    if (result == 0){
      printf ("A timeout occurred.\n");
      tNum--;
      int elapsedTime = lowestTimedNode->timer; // save amount of time elasped from last call
      // delete info from main node struct and fd set
      FD_CLR (lowestTimedNode->sockDes, &masterset); // clear that sock from the master fd set
      endNode (lowestTimedNode); // remove it from array of P or O nodes
      refreshTimers(elapsedTime); // update the timers of nodes with active timers and the node with the lowest timer
    }

    // if something is ready, figure out which sockets are ready to read
    if (result > 0){
      // CHECK LISTENING SOCKETS
      if (FD_ISSET (participant_sock, &readset)){
        int newSock = newParticipant (); // add a new participant if something changed on this socket
        if (newSock != 0){ // if the newParticipant didn't quit
          FD_SET (newSock, &masterset); // adds the new participant sock to the masterset
          nfds = newSock > nfds ? newSock : nfds; // update largest socket num if possible
          //printf("New participant at socket: %d\n", pSockArray[pNum-1].sockDes);
        }
      }
      if (FD_ISSET (observer_sock, &readset)){
        int newSock = newObserver (); // add a new observer if something changed on this socket
        if (newSock != 0){ // if the newObserver didn't quit
          FD_SET (newSock, &masterset);
          nfds = newSock > nfds ? newSock : nfds; // update largest socket num if possible
        }
      }

      // CHECK PARTICIPANT SOCKETS
      for (int k = 0; k < pNum; k++){ // go through pSockArray
        if (FD_ISSET (pSockArray[k].sockDes, &readset)){ // if one of them is ready to be read
          int quitCheck = 0;
          // MAKE USER NAME PROTOCOL
          if (pSockArray[k].state == 1){
            if ((quitCheck = makePcptUserName (&pSockArray[k])) == 0){
              // if participant hung up, clear them from memory
              FD_CLR (pSockArray[k].sockDes, &masterset); // clear that sock from the master fd set
              endNode (&pSockArray[k]); // get rid of that socket info in the struct
            }
          }
          // SEND MESSAGES PROTOCOL
          else if (pSockArray[k].state == 2){
            if ((quitCheck = processPtcptMsg (&pSockArray[k])) == 0){
              // if a participant hung up, clear them from memory
              int theirObserver = pSockArray[k].obsvSock;
              printf("%s hung up\n", pSockArray[k].pName);
              FD_CLR (pSockArray[k].sockDes, &masterset);
              endNode (&pSockArray[k]);
              // if the quitting participant was affiliated with an observer, they quit too
              if (theirObserver != 0){
                for (int i = 0; i < oNum; i++){
                  if (oSockArray[i].sockDes == theirObserver){
                    FD_CLR (theirObserver, &masterset);
                    endNode (&oSockArray[i]);
                  }
                }
              }
            }
          }
        }
      }

      // CHECK OBSERVER SOCKETS
      for (int k = 0; k < oNum; k++){ // go through oSockArray
        if (FD_ISSET (oSockArray[k].sockDes, &readset)){ // if one of them is ready to be read
          int quitCheck = 1;
          // AFFILIATE OBSERVER W/ PARTICIPANT
          if (oSockArray[k].state == 1){
            if ((quitCheck = affiliateObserver (&oSockArray[k])) == 0){
              // if a observer hung up, clear them from memory
              FD_CLR (oSockArray[k].sockDes, &masterset); // clear that sock from the fd set
              endNode (&oSockArray[k]); // get rid of that socket info in the struct
            }
          }
          // CHANGE OF OBSERVER STATE (either they accidently sent something or quit)
          else if (oSockArray[k].state == 2){
            //if ((quitCheck = observerChange (&oSockArray[k])) == 0){
              // if a observer hung up, clear them from memory
              //printf("%s hung up\n", oSockArray[k].pName);
            pSockArray[oSockArray[k].prtSockIdx].obsvSock = 0;
            FD_CLR (oSockArray[k].sockDes, &masterset);
            endNode (&oSockArray[k]);

          }
        }
      }
    }
  }

}
