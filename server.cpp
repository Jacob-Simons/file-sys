#include <iostream>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "FileSys.h"
#include <cstring>
#include <unistd.h>

using namespace std;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: ./nfsserver port#\n";
        return -1;
    }
    int port = atoi(argv[1]);
    //
    //networking part: create the socket and accept the client connection
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    //address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //creating socket
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
      {
        perror("sock");
        exit(1);
      }

    //binding
    if(bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0)
      {
        perror("bind");
        exit(1);
      }

    //listening
    if(listen(sock, 1) < 0)
      {
        perror("listen");
        exit(1);
      }

    socklen_t addrlen = sizeof(address);
    int newsock;
    if((newsock = accept(sock, (struct sockaddr*)&address, &addrlen)) < 0)
      {
        perror("accept");
        exit(1);
      }
   
    //mount the file system
    FileSys fs;
    fs.mount(newsock); //assume that sock is the new socket created 
                    //for a TCP connection between the client and the server.   
    //loop: get the command from the client and invoke the file
    bool quit = false;
    while(!quit)
      {
    bool done = false;
    char buffer[8000] = {0};
    char* buffer_ptr = buffer;
    int bytes_read = 0;
    int total_bytes_read = 0;
    while(!done)
      {
        bytes_read =  recv(newsock, (void*)buffer_ptr, sizeof(buffer), 0);        
        if(bytes_read == 0)
          done = true;
        if(done)
          break;
        total_bytes_read += bytes_read;
        for(int i = 0; i < total_bytes_read; i++)
          {
            if(buffer[i] == '\\')
              {
                done = true;
                break;
              }
          }
        buffer_ptr += bytes_read;
      }
    string msg;
    for(int i = 0; i < total_bytes_read; i++)
      {
        msg += buffer[i];
      }
    msg.erase((int)msg.size()-4, 4);
    size_t index = msg.find(" ");
    string cmd;
    string name;
    string data;
    bool name_set = false;
    bool data_set = false;
    
    if(index != string::npos)
      cmd = msg.substr(0, (int)index);
    else
      cmd = msg.substr(0, msg.size());
    if(index != string::npos)
      {
        int old_index = index;
        index = string::npos;
        index = msg.find(" ", old_index + 1);
        if(index == string::npos)
          {
            name_set = true;
            name =  msg.substr(old_index + 1, msg.size());
            cout << name << endl;
          }
        else
          {
            name_set = true;
            data_set = true;
            int length = (int)index - (int)old_index - 1;
            name = msg.substr(old_index + 1, length);
            data = msg.substr((int)index + 1, msg.size());
          }
      }
    char* name_char;
    char* data_char;
    if(name_set)
      {
        name_char = new char[name.size() + 1];
        strcpy(name_char, name.c_str());
      }
      if(data_set)
      {
        data_char = new char[data.size() + 1];
        strcpy(data_char, data.c_str());
      }
  
    if(cmd == "mkdir")
      fs.mkdir(name_char);
    else if(cmd == "cd")
      fs.cd(name_char);
    else if(cmd == "ls")
      fs.ls();
    else if(cmd == "home")
      fs.home();
    else if(cmd == "create")
      fs.create(name_char);
    else if(cmd == "append")
      fs.append(name_char, data_char);
    else if(cmd == "stat")
      fs.stat(name_char);
    else if(cmd == "cat")
      fs.cat(name_char);
    else if(cmd == "head")
      fs.head(name_char, atoi(data_char));
    else if(cmd == "rm")
      fs.rm(name_char);
    else if(cmd == "rmdir")
      fs.rmdir(name_char);
    
    if(name_set)
      delete[] name_char;
    if(data_set)
      delete[] data_char;
      }
    //system operation which returns the results or error messages back to the clinet
    //until the client closes the TCP connection.


    //close the listening socket
    //close(newsock);
    //unmout the file system
    fs.unmount();
    return 0;
}
