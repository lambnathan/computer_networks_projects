// **************************************************************************************
// * Echo Strings (echo_s.cc)
// * -- Accepts TCP connections and then echos back each string sent.
// **************************************************************************************
#include "web_server.h"

int listenFd;
int connFd;

void my_handler(sig_t s){
  printf("Caught signal %d\n", s);
  close(listenFd);
  close(connFd);
  printf("Closing connections\n");
  exit(1);
}

// **************************************************************************************
// * processConnection()
// process an html request
// read the header and determine if it is a valid request
// if valid, write the desired file back to the client
// otherwise, send a response indicating the error
// **************************************************************************************
void processConnection(int sockFd) {
  DEBUG << "processing the request" << ENDL;

  //first, read the entire header
  int actual_size = 10;
  int buffer_size = 10; //read this many chars at a time
  int current_pos = 0;
  char* header = (char*)malloc(buffer_size + 1);
  header[0] = '\0';
  while (true) {
    //
    // first read in the header 
    //
    char* part = (char*)malloc(sizeof(char) * (buffer_size)); //buffer_size of actual data
    int bytes_read = read(sockFd, part, buffer_size); 
    if(bytes_read < 0){
      std::cout << "An error occurred in the read." << std::endl;
	    free(part);
      free(header);
      return;
    }
    if(bytes_read == 0){
      DEBUG << "End of file reached" << ENDL;
      free(part);
      break;
    }
    DEBUG << "Read " << bytes_read << " bytes from the client" << ENDL;
    //copy the read data into the buffer. strcat find current null terminator, overwrites it, then places new one at end of catted string
    memcpy(&header[current_pos], part, bytes_read);
    //check if reached the end of the request
    if(bytes_read != buffer_size || strstr(part, "\r\n\r\n") != nullptr){ //reached the end of the file
      DEBUG << "Found the last line" << ENDL;
      free(part);
      break;
    }
    //increase the actual buffer size
    actual_size += bytes_read;
    current_pos += bytes_read;
    //reallocate the header for the header0
    DEBUG << "reallocting to size " << actual_size << ENDL;
    header = (char*)realloc(header, actual_size);

    //free the part char*
    free(part);
  }
  //done reading the header. now print it out

  std::cout << "Start of header:" << std::endl;
  std::cout << header << std::endl;
  std::cout << "end of header" << std::endl;
  // if(strstr(header, "\r\n\r\n") != nullptr){
  //   std::cout << "actually found end of header!" << std::endl;
  // }


  //determine if the request is valid
  //must be a GET request, and be requesting either /fileX.html or /imageX.html
  //where X is a single digit number 0-9
  DEBUG << "Starting search for the filename" << ENDL;
  if(strncmp(header, "GET", 3) != 0){ //not a valid get request. respond with a 400 error
    DEBUG << "Not a valid GET request" << ENDL;
    char bad_request[] = "HTTP/1.0 400 Bad Request\r\n\r\n";
    write(connFd, bad_request, strlen(bad_request));
    free(header);
    return;
  }
  regex_t regex1;
  regex_t regex2;
  int ret1;
  int ret2;
  ret1 = regcomp(&regex1, "GET /file[0-9].html", 0);
  ret2 = regcomp(&regex2, "GET /image[0-9].jpg", 0);
  if(ret1 || ret2){
    std::cout << "error: could not compile regex" << std::endl;
    free(header);
    return;
  }
  char* to_compare_file = (char*)malloc(16);
  char* to_compare_image = (char*)malloc(16);
  memcpy(to_compare_file, header, 15);
  memcpy(to_compare_image, header, 15);
  to_compare_file[15] = '\0';
  to_compare_image[15] = '\0';
  ret1 = regexec(&regex1, to_compare_file, 0, NULL, 0);
  ret2 = regexec(&regex2, to_compare_image, 0, NULL, 0);
  if(!ret1 || !ret2){ //requesting a valid filename (exact file could still not exist though)
    //get file name and try to open the file
    char filename[11];
    memcpy(filename, &header[5], 10);
    filename[10] = '\0';
    FILE *fileptr;
    char* buffer;
    fileptr = fopen(filename, "rb");
    if(fileptr == nullptr){ //requested file actually does not exist
      DEBUG << "Valid GET request, but invalid file" << ENDL;
      char not_found[] = "HTTP/1.0 404 Not Found\r\n\r\n";
      write(connFd, not_found, strlen(not_found));
    }
    else{ //actual file does exist, so we read it
      DEBUG << "Valid GET request for " << filename << ENDL;
      long int file_len;
      fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
      file_len = ftell(fileptr);             // Get the current byte offset in the file
      rewind(fileptr);                      // Jump back to the beginning of the file
      buffer = (char *)malloc(file_len * sizeof(char)); // Enough memory for the file
      fread(buffer, file_len, 1, fileptr); // Read in the entire file
      fclose(fileptr); // Close the file
      DEBUG << "Successfully read the requested file into the buffer" << ENDL;

      //now send header
      char status_line[] = "HTTP/1.0 200 OK\r\n";
      DEBUG << "Sending header" << ENDL;
      write(connFd, status_line, strlen(status_line));
      
      //send the file size
      int length = snprintf(NULL, 0, "%d", file_len); //convert int to string
      char* to_string = (char*)malloc(length + 1);
      snprintf(to_string, length + 1, "%d", file_len);//converting size to string
      char* content_length_line = (char*)malloc(length + 16 + 2); //size of integer file size + size of "Content-Length: "
      content_length_line[0] = '\0';
      content_length_line = strcat(content_length_line, "Content-Length: ");
      content_length_line = strcat(content_length_line, to_string);
      content_length_line = strcat(content_length_line, "\r\n");
      DEBUG << "Sending Content-Length of: " << file_len << ENDL;
      write(connFd, content_length_line, length + 16 + 2);
      free(content_length_line);
      free(to_string);

      //send the content type depending on file requested
      if(strstr(filename, ".html") != NULL){//content type is html/text
        char content_type[] = "Content-Type: text/html\r\n\r\n";
        DEBUG << "Sending Content-Type of: text/html" << ENDL; 
        write(connFd, content_type, strlen(content_type));
      }
      else if(strstr(filename, ".jpg") != NULL){//content type is an image
        char content_type[] = "Content-Type: image/jpeg\r\n\r\n";
        DEBUG << "Sending Content-Type of: image/jpeg" << ENDL;
        write(connFd, content_type, strlen(content_type));
      }


      //finally, send the requested file
      DEBUG << "Sending all the data to the client" << ENDL;
      write(connFd, buffer, file_len);
      free(buffer);
    } 
  }
  else{//not a valid file name scheme
    char not_found[] = "HTTP/1.0 404 Not Found\r\n\r\n";
    DEBUG << "Not a valid file name" << ENDL;
    write(connFd, not_found, strlen(not_found));
  }

  //free stuff before returning
  free(to_compare_file);
  free(to_compare_image);
  free(header);
  regfree(&regex1);
  regfree(&regex2);
  return;
}
    


// **************************************************************************************
// * main()
// * - Sets up the sockets and accepts new connection until processConnection() returns 1
// **************************************************************************************

int main (int argc, char *argv[]) {

  //create signal handler for catching CTRL-C
  signal (SIGINT, (__sighandler_t)my_handler);


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
  listenFd = -1;
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
  while (true) {
    connFd = -1;
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
    processConnection(connFd);
    DEBUG << "returned from processing the connection" << ENDL;
    close(connFd);
  }

}
