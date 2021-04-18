// **************************************************************************************
// * Echo Strings (echo_s.cc)
// * -- Accepts TCP connections and then echos back each string sent.
// **************************************************************************************
#include "echo_s.h"


// **************************************************************************************
// * processConnection()
// * - Handles reading the line from the network and sending it back to the client.
// * - Returns 1 if the client sends "QUIT" command, 0 if the client sends "CLOSE".
// **************************************************************************************
int processConnection(int sockFd) {

  int quitProgram = 0;
  int keepGoing = 1;
  while (keepGoing) {

    //
    // Call read() call to get a buffer/line from the client.
    // Hint - don't forget to zero out the buffer each time you use it.
    //
    char buffer[1024];
    int bytes_read = read(sockFd, buffer, sizeof(buffer));
    if(bytes_read < 0){
      std::cout << "An error occurred in the read." << std::endl;
	    exit(-1);
    }
    DEBUG << "Recieved a block of data of " << bytes_read << " bytes." << ENDL;
  
    //
    // Check for one of the commands (CLOSE or QUIT)
    //
    if(strncmp(buffer, "CLOSE", 5) == 0){ //user entered CLOSE at the beginning of message
      //close connection file descriptor and exit function, returning 0
      DEBUG << "CLOSE command was entered." << ENDL;
   	  close(sockFd);
	    return 0;
    }
    if(strncmp(buffer, "QUIT", 4) == 0){ //user entered QUIT at the beginning of message
      //close connection file descriptor and exit function, returning 0
	    DEBUG << "QUIT command was entered." << ENDL;
	    close(sockFd);
	    return 1;
    }

    //
    // Call write() to send line back to the client.
    //
    write(sockFd, buffer, bytes_read);

  }

  return quitProgram;
}
    


// **************************************************************************************
// * main()
// * - Sets up the sockets and accepts new connection until processConnection() returns 1
// **************************************************************************************

int main (int argc, char *argv[]) {

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  boost::log::add_console_log(std::cout, boost::log::keywords::format = "%Message%");
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  int opt = 0;
  while ((opt = getopt(argc,argv,"v")) != -1) {
    
    switch (opt) {
    case 'v':
      boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
      break;
    case ':':
    case '?':
    default:
      std::cout << "useage: " << argv[0] << " -v" << std::endl;
      exit(-1);
    }
  }

  // *******************************************************************
  // * Creating the inital socket is the same as in a client.
  // ********************************************************************
  int     listenFd = -1;
       // Call socket() to create the socket you will use for lisening.

  if((listenFd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    std::cout << "Failed to create listening socket " << strerror (errno) << std::endl;
    exit(-1);
  }
  DEBUG << "Calling Socket() assigned file descriptor " << listenFd << ENDL;

  
  // ********************************************************************
  // * The bind() and calls take a structure that specifies the
  // * address to be used for the connection. On the cient it contains
  // * the address of the server to connect to. On the server it specifies
  // * which IP address and port to lisen for connections.
  // ********************************************************************
  struct sockaddr_in servaddr;
  srand(time(NULL));
  int port = (rand() % 10000) + 1024; //pick a random high-numbered port
  bzero(&servaddr, sizeof(servaddr)); //zero the whole thing initially
  servaddr.sin_family = PF_INET; //IPv4 protocol family
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //letting the system pick the IP address
  servaddr.sin_port = htons(port);


  // ********************************************************************
  // * Binding configures the socket with the parameters we have
  // * specified in the servaddr structure.  This step is implicit in
  // * the connect() call, but must be explicitly listed for servers.
  // ********************************************************************
  DEBUG << "Calling bind(" << listenFd << "," << &servaddr << "," << sizeof(servaddr) << ")" << ENDL;
  int bindSuccesful = 0;
  bindSuccesful = bind(listenFd, (sockaddr*) &servaddr, sizeof(servaddr));
  while (bindSuccesful != 0) {
    // You may have to call bind multiple times if another process is already using the port
    // your program selects.
    port = (rand() % 10000) + 1024; //pick a random high-numbered port
    servaddr.sin_port = htons(port);
    bindSuccesful = bind(listenFd, (sockaddr*) &servaddr, sizeof(servaddr)); //call bind again
  }
  std::cout << "Using port " << port << std::endl;


  // ********************************************************************
  // * Setting the socket to the listening state is the second step
  // * needed to being accepting connections.  This creates a queue for
  // * connections and starts the kernel listening for connections.
  // ********************************************************************
  int listenQueueLength = 1;
  DEBUG << "Calling listen(" << listenFd << "," << listenQueueLength << ")" << ENDL;
  if(listen(listenFd, listenQueueLength) < 0){
    std::cout << "listen() failed: " << strerror(errno) << std::endl;
    exit(-1);
  }

  // ********************************************************************
  // * The accept call will sleep, waiting for a connection.  When 
  // * a connection request comes in the accept() call creates a NEW
  // * socket with a new fd that will be used for the communication.
  // ********************************************************************
  int quitProgram = 0;
  while (!quitProgram) {
    int connFd = -1;
    DEBUG << "Calling accept(" << listenFd << "NULL,NULL)." << ENDL;
    if((connFd = accept(listenFd, (sockaddr *) NULL, NULL)) < 0){
      std::cout << "accept() failed: " << strerror(errno) << std::endl;
      exit(-1);
    }

    // The accept() call checks the listening queue for connection requests.
    // If a client has already tried to connect accept() will complete the
    // connection and return a file descriptor that you can read from and
    // write to. If there is no connection waiting accept() will block and
    // not return until there is a connection.
    
    DEBUG << "We have recieved a connection on " << connFd << ENDL;

    
    quitProgram = processConnection(connFd);
   
    close(connFd);
  }

  close(listenFd);

}
