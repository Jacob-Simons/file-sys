// CPSC 3500: Shell
// Implements a basic shell (command line interface) for the file system

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
using namespace std;

#include "Shell.h"

static const string PROMPT_STRING = "NFS> ";	// shell prompt

// Mount the network file system with server name and port number in the format of server:port
void Shell::mountNFS(string fs_loc) {
	//create the socket cs_sock and connect it to the server and port specified in fs_loc
	//if all the above operations are completed successfully, set is_mounted to true  

  //if((cs_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   //{
   //  perror("socket");
   //}

  //splitting into address and port
  size_t index = -1;
  index = fs_loc.find(":");
  if(index == -1)
    {
      //error improper
    }

  string address = fs_loc.substr(0, index);
  
  string port = fs_loc.substr(index +1, fs_loc.size());
  //have to free returned from getaddrinfo
  //struct sockaddr_in server;
  struct addrinfo hints, *servinfo, *p;
  memset(&hints, 0, sizeof(hints));
  //hints.ai_family = AF_INET;
  //hints.ai_socktype = SOCK_STREAM;
  //hints.ai_protocol = IPPROTO_TCP;
  const char* port_char = port.c_str();
  const char* address_char = address.c_str();

  int rv;
  if((rv = getaddrinfo(address_char, port_char, &hints, &servinfo)) != 0)
    {
      perror("getaddrinfo");
    }
  
  for(p = servinfo; p != NULL; p = p -> ai_next)
    {
      if((cs_sock = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1)
        {
          perror("socket");
          continue;
        }

      if(connect(cs_sock, p -> ai_addr, p -> ai_addrlen) == -1)
        {
          perror("connect");
          close(cs_sock);
          continue;
        }
      //int a = 9;
      //int ch = send(cs_sock,&a,sizeof(int), 0);
      break;
    }
  
  //server.sin_family = AF_INET;
  //server.sin_port = htons(stoi(port));

  //if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
  //{
  //  perror("connect");
  //}
  is_mounted = true;

}

// Unmount the network file system if it was mounted
void Shell::unmountNFS()
{
  close(cs_sock);
  return;
}

// Remote procedure call on mkdir
void Shell::mkdir_rpc(string dname)
{
  string msg = "mkdir ";
  msg += dname;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on cd
void Shell::cd_rpc(string dname)
{
  string msg = "cd ";
  msg += dname;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on home
void Shell::home_rpc()
{
  string msg = "home\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on rmdir
void Shell::rmdir_rpc(string dname)
{
  string msg = "rmdir ";
  msg += dname;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on ls
void Shell::ls_rpc()
{
  string msg = "ls\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on create
void Shell::create_rpc(string fname)
{
  string msg = "create ";
  msg += fname;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on append
void Shell::append_rpc(string fname, string data)
{
  cout << "fname" << fname << endl;
  string msg = "append ";
  msg += fname;
  msg += " ";
  msg += data;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procesure call on cat
void Shell::cat_rpc(string fname)
{
  head_rpc(fname, 10000);
}

                            

// Remote procedure call on head
void Shell::head_rpc(string fname, int n)
{
  string msg = "head ";
  msg += fname;
  msg += " ";
  msg += to_string(n);
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on rm
void Shell::rm_rpc(string fname)
{
  string msg = "rm ";
  msg += fname;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Remote procedure call on stat
void Shell::stat_rpc(string fname)
{
  string msg = "stat ";
  msg += fname;
  msg += "\\r\\n";
  send_recv_cmd(msg.c_str());
}

// Executes the shell until the user quits.
void Shell::run()
{
  // make sure that the file system is mounted
  if (!is_mounted)
 	return; 
  // continue until the user quits
  bool user_quit = false;
  while (!user_quit) {

    // print prompt and get command line
    string command_str;
    cout << PROMPT_STRING;
    getline(cin, command_str);

    // execute the command
    user_quit = execute_command(command_str);
  }

  // unmount the file system
  unmountNFS();
}

// Execute a script.
void Shell::run_script(char *file_name)
{
  // make sure that the file system is mounted
  if (!is_mounted)
  	return;
  // open script file
  ifstream infile;
  infile.open(file_name);
  if (infile.fail()) {
    cerr << "Could not open script file" << endl;
    return;
  }


  // execute each line in the script
  bool user_quit = false;
  string command_str;
  getline(infile, command_str, '\n');
  while (!infile.eof() && !user_quit) {
    cout << PROMPT_STRING << command_str << endl;
    user_quit = execute_command(command_str);
    getline(infile, command_str);
  }

  // clean up
  unmountNFS();
  infile.close();
}


// Executes the command. Returns true for quit and false otherwise.
bool Shell::execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);

  // look for the matching command
  if (command.name == "") {
    return false;
  }
  else if (command.name == "mkdir") {
    mkdir_rpc(command.file_name);
  }
  else if (command.name == "cd") {
    cd_rpc(command.file_name);
  }
  else if (command.name == "home") {
    home_rpc();
  }
  else if (command.name == "rmdir") {
    rmdir_rpc(command.file_name);
  }
  else if (command.name == "ls") {
    ls_rpc();
  }
  else if (command.name == "create") {
    create_rpc(command.file_name);
  }
  else if (command.name == "append") {
    append_rpc(command.file_name, command.append_data);
  }
  else if (command.name == "cat") {
    cat_rpc(command.file_name);
  }
  else if (command.name == "head") {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno) {
      head_rpc(command.file_name, n);
    } else {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm") {
    rm_rpc(command.file_name);
  }
  else if (command.name == "stat") {
    stat_rpc(command.file_name);
  }
  else if (command.name == "quit") {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Shell::Command Shell::parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name) {
    num_tokens++;
    if (ss >> command.file_name) {
      num_tokens++;
      if (ss >> command.append_data) {
        num_tokens++;
        string junk;
        if (ss >> junk) {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0) {
    return empty;
  }
    
  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
      command.name == "cd"    ||
      command.name == "rmdir" ||
      command.name == "create"||
      command.name == "cat"   ||
      command.name == "rm"    ||
      command.name == "stat")
  {
    if (num_tokens != 2) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl; 
    return empty;
  } 

  return command;
}


void Shell::send_recv_cmd(const char* cmd)
{
  int size = strlen(cmd);
  int bytes_sent = 0;
  int total_bytes_sent = 0;
  while(total_bytes_sent < size)
    {
      bytes_sent = send(cs_sock, (void*)cmd, size - total_bytes_sent, 0);
      total_bytes_sent += bytes_sent;
      cmd += bytes_sent;
    }

  char buffer[8000];
  char* buffer_ptr = buffer;
  int total_bytes_recv = 0;
  int total_msg_len = 0;
  bool len_found = false;
  bool done = false;
  string num;
  int last_num;
  int byte_recv;
  while(!done)
    {
      if(!len_found)
        {
          byte_recv = recv(cs_sock, (void*)buffer_ptr, sizeof(buffer), 0);
        }
        else
        {
           while(total_bytes_recv < last_num - 1 + 8 + total_msg_len)
            {
              byte_recv = recv(cs_sock, (void*)buffer_ptr, sizeof(buffer), 0);
              total_bytes_recv += byte_recv;
              buffer_ptr += byte_recv;
            }
          done = true;
        }
      if(!done)
        total_bytes_recv += byte_recv;
      buffer_ptr += byte_recv;
      if(!len_found)
        {
           for(int i = 0; i < total_bytes_recv; i++)
            {
               if(buffer[i] == 'L' && buffer[i+1] == 'e' && buffer[i+2] == 'n' && buffer[i+3] == 'g' && buffer[i+4] == 't' && buffer[i+5] == 'h' )
                 {
                   int first_num = i+7;
                   for(int j = first_num; j < total_bytes_recv; j++)
                    {
                      if(buffer[j] == '\\')
                        {
                          last_num = j;
                          for(int k = first_num; k < j; k++)
                            {
                              num += buffer[k];
                            }
                          total_msg_len = stoi(num);
                          len_found = true;
                          break;
                        }
                    }
                  break;
                 }
            }
        }
    }

  for(int i = 0; i < total_bytes_recv; i++)
    {
      if(buffer[i] == '\\')
        {
          i += 3;
          cout << endl;
        }
      else
        cout << buffer[i];
    }
  if(total_msg_len > 0)
    cout << endl;

}
