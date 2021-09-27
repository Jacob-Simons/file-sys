// CPSC 3500: File System
// Implements the file system commands that are available to the shell.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <math.h>
using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

// mounts the file system
void FileSys::mount(int sock) {
  bfs.mount();
  curr_dir = 1; //by default current directory is home directory, in disk block #1
  fs_sock = sock; //use this socket to receive file system operations from the client and send back response messages
}

// unmounts the file system
void FileSys::unmount() {
  bfs.unmount();
  close(fs_sock);
}

// make a directory
void FileSys::mkdir(const char *name)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void*)&dir_block);

  if(dir_block.num_entries == MAX_DIR_ENTRIES)
    {
      const char* msg = "506 Directory is full\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  
  else if(strlen(name) > 9)
    {
      const char* msg = "504 File name is too long\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;

    }
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if( strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          const char* msg = "502 File exists\\r\\n";
          const char* data = "";
          send_response(msg, data);
          return;

        }
    }

  if(dir_block.num_entries == 0 && curr_dir == 1)
    {
      for(int i = 0; i < MAX_DIR_ENTRIES; i++)
        {
          dir_block.dir_entries[i].name[0] = '\0';
        }
    }

  int free_dir_entry;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(dir_block.dir_entries[i].name[0] == '\0')
        {
          free_dir_entry = i;
          break;
        }
    }
  strcpy(dir_block.dir_entries[free_dir_entry].name, name);

  short free_block = bfs.get_free_block();
  if(free_block == 0)
    {
      const char* msg = "505 Disk is full\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

  dir_block.dir_entries[free_dir_entry].block_num = free_block;
  struct dirblock_t new_dir;
  new_dir.magic = DIR_MAGIC_NUM;
  new_dir.num_entries = 0;

  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      new_dir.dir_entries[i].name[0] = '\0';
    }
  
  bfs.write_block(dir_block.dir_entries[free_dir_entry].block_num, &new_dir);

  dir_block.num_entries++;
  bfs.write_block(curr_dir, &dir_block);

  const char* msg = "200 OK\\r\\n";
  const char* data = "";
  send_response(msg, data);
  return;
}

// switch to a directory
void FileSys::cd(const char *name)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void*)&dir_block);
  
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          struct dirblock_t temp_block;
          bfs.read_block(dir_block.dir_entries[i].block_num, (void*)&temp_block);
          if(temp_block.magic != DIR_MAGIC_NUM)
            {
              const char* msg = "500 File is not a directory\\r\\n";
              const char* data = "";
              send_response(msg, data);
              return;
            }
          curr_dir = dir_block.dir_entries[i].block_num;
          break;
        }
      if(i == MAX_DIR_ENTRIES - 1)
        {
          const char* msg = "503 File does not exist\\r\\n";
          const char* data = "";
          send_response(msg, data);
          return;
        }
    }
  const char* msg = "200 OK\\r\\n";
  const char* data = "";
  send_response(msg, data);
  return;
}

// switch to home directory
void FileSys::home()
{
  curr_dir = 1;
  const char* msg = "200 OK\\r\\n";
  const char* data = "";
  send_response(msg, data);
}

// remove a directory
void FileSys::rmdir(const char *name)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, &dir_block);

  //searching for dir name
  int found = -1;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          found = i;
          break;
        }
    }

  if(found == -1)
    {
      const char* msg = "503 File does not exist\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

  //reading delete directory
  struct dirblock_t delete_block;
  bfs.read_block(dir_block.dir_entries[found].block_num, &delete_block);
  if(delete_block.num_entries != 0)
    {
      const char* msg = "507 Directory is not empty\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  else if(delete_block.magic != DIR_MAGIC_NUM)
    {
      const char* msg = "500 File is not a directory\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

  memset(dir_block.dir_entries[found].name, 0, MAX_FNAME_SIZE);
  dir_block.dir_entries[found].name[0] = '\0';
  bfs.reclaim_block(dir_block.dir_entries[found].block_num);
  dir_block.num_entries--;

  bfs.write_block(curr_dir, &dir_block);

  const char* msg = "200 OK\\r\\n";
  const char* data = "";
  send_response(msg, data);
  return;
}


// list the contents of current directory
void FileSys::ls()
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, &dir_block);
  int total = dir_block.num_entries;
  int index = 0;
  int counter = 0;
  string output = "";
  if(total != 0)
    {
      while(counter < total)
        {
          if(dir_block.dir_entries[index].name[0] != '\0')
            {
              dirblock_t temp_block;
              bfs.read_block(dir_block.dir_entries[index].block_num, &temp_block);

              if(temp_block.magic == DIR_MAGIC_NUM)
                {
                  output.append(dir_block.dir_entries[index].name);
                  output.append("/  ");
                }
              else
                {
                  output.append(dir_block.dir_entries[index].name);
                  output.append("  ");
                }
              counter++;
            }
          index++;
        }
    }

  const char* msg = "200 OK\\r\\n";
  const char* data = output.c_str();
  send_response(msg, data);

}

// create an empty data file
void FileSys::create(const char *name)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, &dir_block);

   if(dir_block.num_entries == MAX_DIR_ENTRIES)
    {
      const char* msg = "506 Directory is full\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
   else if(strlen(name) > 9)
    {
      const char* msg = "504 File name is too long\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

   for(int i = 0; i < MAX_DIR_ENTRIES; i++)
        {
          if(strcmp(dir_block.dir_entries[i].name, name) == 0)
            {
              const char* msg = "502 File exists\\r\\n";
              const char* data = "";
              send_response(msg, data);
              return;
            }
        }

   
  if(dir_block.num_entries == 0 && curr_dir == 1)
    {
      for(int i = 0; i < MAX_DIR_ENTRIES; i++)
        {
          dir_block.dir_entries[i].name[0] = '\0';
        }
    }
  
  int free_dir_entry;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(dir_block.dir_entries[i].name[0] == '\0')
        {
          free_dir_entry = i;
          break;
        }
    }
  
  //updating directory
  strcpy(dir_block.dir_entries[free_dir_entry].name, name);
  short free_block = bfs.get_free_block();
  if(free_block == 0)
    {
      const char* msg = "505 Disk is full\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  dir_block.dir_entries[free_dir_entry].block_num = free_block;
  cout << "block num:" <<  dir_block.dir_entries[free_dir_entry].block_num << endl;
  dir_block.num_entries++;

  //creating inode
  struct inode_t new_file;
       //check for available blocks
  new_file.magic = INODE_MAGIC_NUM;
  new_file.size = 0;

  //writing
  bfs.write_block(curr_dir, (void*) &dir_block);
  bfs.write_block(dir_block.dir_entries[free_dir_entry].block_num, (void*) &new_file);

  const char* msg = "200 OK\\r\\n";
  const char* data = "";
  send_response(msg, data);
  return;
}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void*)&dir_block);

  //looking for file
  int found = -1;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          found = i;
          break;
        }
    }
  if(found == -1)
    {
      const char* msg = "503 File does not exist\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  
  //reading file inode
  struct inode_t inode_block;
  bfs.read_block(dir_block.dir_entries[found].block_num, (void*)&inode_block);
  
  int size = inode_block.size;
  inode_block.size += strlen(data);
  //checking if full
  if(size + strlen(data) > BLOCK_SIZE * MAX_DATA_BLOCKS)
    {
      const char* msg = "508 Append exceeds maximum file size\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

  //checking if not inode
  if(inode_block.magic != INODE_MAGIC_NUM)
    {
      const char* msg = "501 File is a directory\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

  int block_index;
  int index;
  bool new_block = false;
  bool first_time = false;
  if(size % 128 == 0)
    {
      block_index = size / 128;
      index = 0;
      new_block = true;
    }
  else
    {
      block_index = size / 128;
      index = size % 128;
      first_time = true;
    }

  int counter = strlen(data);
  while(counter > 0)
    {
      if(new_block)
        {
          short free_block = bfs.get_free_block();
          if(free_block == 0)
            {
              const char* msg = "505 Disk is full\\r\\n";
              const char* data = "";
              send_response(msg, data);
              return;
            }
          inode_block.blocks[block_index] = bfs.get_free_block();                
          cout << "append block:" << inode_block.blocks[block_index] << endl;
        }
      struct datablock_t data_block;
      if(first_time)
        {
          bfs.read_block(inode_block.blocks[block_index], (void*)&data_block);
          first_time = false;
        }
      while(index < 128)
        {
          data_block.data[index] = data[strlen(data) - counter];
          index++;
          counter--;
        }
      index = 0;
      bfs.write_block(inode_block.blocks[block_index], (void*)&data_block);
      new_block = true;
      block_index++;
    }

    
  bfs.write_block(dir_block.dir_entries[found].block_num, (void*)&inode_block);

  const char* msg = "200 OK \\r\\n";
  const char* data_send = "";
  send_response(msg, data_send);
  return;
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
  head(name, BLOCK_SIZE * MAX_DATA_BLOCKS);
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void*)&dir_block);

  int found = -1;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          found = i;
          break;
        }
    }

  if(found == -1)
    {
      const char* msg = "503 File does not exist\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  
  struct inode_t file_inode;
  bfs.read_block(dir_block.dir_entries[found].block_num, (void*)&file_inode);
  if(file_inode.magic != INODE_MAGIC_NUM)
    {
      const char* msg = "501 File is a directory\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  
  int total_size = file_inode.size;

  int index = 0;
  int total_index = 0;
  int block_index = 0;
  string output = "";
  while(total_index < n && total_index < total_size)
    {
      struct datablock_t data_block;
      bfs.read_block(file_inode.blocks[block_index], (void*)&data_block);
      while(index < 128 && total_index < n && total_index < total_size)
        {
          output +=  data_block.data[index];
          index++;
          total_index++;
        }
      index = 0;
      block_index++;
    }

  const char* msg = "200 OK \\r\\n";
  const char* data = output.c_str();
  send_response(msg, data);
  return;
}

// delete a data file
void FileSys::rm(const char *name)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void*)&dir_block);

  int found = -1;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          found = i;
          break;
        }
    }

  if(found == -1)
    {
      const char* msg = "503 File does not exist\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }


  //reclaiming allocated data blocks
  struct inode_t file_inode;
  bfs.read_block(dir_block.dir_entries[found].block_num, (void*)&file_inode);
  if(file_inode.magic != INODE_MAGIC_NUM)
    {
      const char* msg = "501 File is a directory\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }
  int block_count;
  if(file_inode.size / 128 == 0 && file_inode.size % 128 > 0)
    block_count = 1;
  else
    block_count = file_inode.size / 128;

  for(int i = 0; i <  block_count; i++)
    {
      struct datablock_t data_block;
      bfs.read_block(file_inode.blocks[i], (void*)&data_block);
      for(int k = 0; k < 8; k++)
        {
          data_block.data[k] = 0;
        }
      bfs.write_block(file_inode.blocks[i], (void*)&data_block);    
      bfs.reclaim_block(file_inode.blocks[i]);
    }
  
   file_inode.size = 0;
   bfs.write_block(dir_block.dir_entries[found].block_num, (void*)&file_inode);

  
  //removing from dir and reclaiming inode
  dir_block.num_entries--;
  memset(dir_block.dir_entries[found].name, 0, MAX_FNAME_SIZE);
  dir_block.dir_entries[found].name[0] = '\0';
  bfs.reclaim_block(dir_block.dir_entries[found].block_num);
  dir_block.dir_entries[found].block_num = 0;
  bfs.write_block(curr_dir, (void*)&dir_block);

  const char* msg = "200 OK \\r\\n";
  const char* data = "";
  send_response(msg, data);
  return;

}

// display stats about file or directory
void FileSys::stat(const char *name)
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void*)&dir_block);

  int found = -1;
  for(int i = 0; i < MAX_DIR_ENTRIES; i++)
    {
      if(strcmp(dir_block.dir_entries[i].name, name) == 0)
        {
          found = i;
          break;
        }
    }

  if(found == -1)
    {
      const char* msg = "503 File does not exist\\r\\n";
      const char* data = "";
      send_response(msg, data);
      return;
    }

  struct dirblock_t target_block;
  bfs.read_block(dir_block.dir_entries[found].block_num, (void*)&target_block);
  string output = "";
  if(target_block.magic == DIR_MAGIC_NUM)
    {
      output +=  "Directory name: ";
      output += dir_block.dir_entries[found].name;
      output += "\n";
      output += "Directory block: ";
      output += to_string(dir_block.dir_entries[found].block_num);
      output += "\n";
    }
  else
    {
      struct inode_t inode_block;
      bfs.read_block(dir_block.dir_entries[found].block_num, (void*)&inode_block);

      output += "Inode block: ";
      output += to_string(dir_block.dir_entries[found].block_num);
      output += "\n";
      output += "Bytes in file: ";
      output += to_string(inode_block.size);
      output += "\n";
      int num_blocks;
      if(inode_block.size % 128 == 0)
        {
          if(inode_block.size == 0)
            num_blocks = 0;
          else
            num_blocks = inode_block.size / 128;
        }
      else
        {
          num_blocks = inode_block.size / 128 + 1;
        }
      output +=  "Number of blocks: ";
      output += to_string(num_blocks);
      output += "\n";
      output += "First block: ";
      output += to_string(inode_block.blocks[0]);
      output += "\n";
    }

  const char* msg = "200 OK \\r\\n";
  const char* data = output.c_str();
  send_response(msg, data);
  return;
}

// HELPER FUNCTIONS (optional)
void FileSys::send_response(const char* response, const char* data)
{
  char* msg;
  string temp;
  msg = new char[8000];
  strcpy(msg, response );
  char* msg_ptr = msg;
  cout << "strnlen" << strlen(msg) << endl;
  int bytes_sent = 0;
  int total_bytes_sent = 0;
  while(total_bytes_sent < strlen(msg))
    {
      bytes_sent = send(fs_sock, (void*)msg_ptr, strlen(msg) - total_bytes_sent, 0);
      total_bytes_sent += bytes_sent;
      msg_ptr += bytes_sent;
    }
  msg_ptr = msg;
  memset(msg, 0, sizeof(msg));

  temp = "Length:";
  temp.append(to_string(strlen(data)));
  temp.append("\\r\\n\\r\\n");
  cout << "temp" << temp << endl;
  strcpy(msg, temp.c_str());
  bytes_sent = 0;
  total_bytes_sent = 0;
   while(total_bytes_sent < strlen(msg))
     {
       bytes_sent = send(fs_sock, (void*)msg_ptr, strlen(msg) - total_bytes_sent, 0);
       total_bytes_sent += bytes_sent;
       msg_ptr += bytes_sent;
     }
  memset(msg, 0, sizeof(msg));
  msg_ptr = msg;

  if(strlen(data) != 0)
    {
      strcpy(msg, data);
      bytes_sent = 0;
      total_bytes_sent = 0;
      while(total_bytes_sent < strlen(msg))
        {
          bytes_sent = send(fs_sock, (void*)msg_ptr, strlen(msg) - total_bytes_sent, 0);
          total_bytes_sent += bytes_sent;
          msg_ptr += bytes_sent;
        }
    }
  delete[] msg;
  return;
}
