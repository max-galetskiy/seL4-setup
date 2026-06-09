#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>

#define MAX_NAME_LEN 32
#define MAX_PW_LEN 128
#define MAX_CONTENT_LEN 512

#define BUFFER_LENGTH 4096

#define MAX_USER_AMOUNT 16
#define MAX_FILE_AMOUNT 256

#define INVALID_CHAR (-1)
#define CLIENT_CHANNEL_ID 1

//  ============================
//  Microkit Variables
//  ============================

uintptr_t request_type;
uintptr_t request_first_parameter;
uintptr_t request_second_parameter;
uintptr_t server_output;

//  ============================
//  Application Variables
//  ============================

struct user
{
  char name[MAX_NAME_LEN];
  char pw[MAX_PW_LEN];
};

struct file
{
  char name[MAX_NAME_LEN];
  char owner[MAX_NAME_LEN];
  char content[MAX_CONTENT_LEN];
};

int next_user = 0;
int next_file = 0;

struct user user_list[MAX_USER_AMOUNT];
struct file file_list[MAX_FILE_AMOUNT];

bool logged_in = false;
char current_user[MAX_NAME_LEN];

//  ============================
//  Utility Functions
//  ============================

bool eq(char* a, char* b){

  if(!a || !b){
    return false;
  }

  int i = 0;
  while((a[i] != '\0') && (b[i] != '\0')){
    if(a[i] != b[i]){
      return false;
    }
    i++;
  }
  
  if(a[i] == b[i]){
    return true;
  }
  
  return false;
}

void copy(char* src, char* dest, int max_len){

  int i = 0;
  while(src[i] != '\0' && i < (max_len-1)){
    dest[i] = src[i];
    i++;
  }
  
  dest[i] = '\0';  
}

//  ============================
//  User Management Functions
//  ============================

int find_user(char* u){

  for(int i = 0; i < next_user; i++){
      if(eq(user_list[i].name, u)){
        return i;
      }
  }
  
  return -1;

}

void create_user(char* username, char* pw){

  if(!username || username[0] == '\0'){
      copy("Empty username given!",(char*)server_output, BUFFER_LENGTH);
      return;
  }

  if(find_user(username) >= 0){
      copy("User already exists!\n",(char*)server_output, BUFFER_LENGTH);
      return;
  }
  
  if(!pw || pw[0] == '\0'){
      copy("Empty password given!",(char*)server_output, BUFFER_LENGTH);
      return;
  }
  
  if(next_user == MAX_USER_AMOUNT){
      copy("Max. amount of users reached!",(char*)server_output, BUFFER_LENGTH);
      return;
  }

  copy(username, user_list[next_user].name, MAX_NAME_LEN);
  copy(pw, user_list[next_user].pw, MAX_PW_LEN);
  next_user += 1;
  
  copy("User created.",(char*)server_output, BUFFER_LENGTH);
 
}

void logout(){

  if(!logged_in){
  copy("Not logged in!",(char*)server_output, BUFFER_LENGTH);
  return;
  }

  logged_in = false;
  copy("Logged out.",(char*)server_output, BUFFER_LENGTH);
}

void login(char* username, char* pw){

  logged_in = false;
  
  if(!username || username[0] == '\0'){
    copy("Empty username given!",(char*)server_output, BUFFER_LENGTH);
    return;
  }
  
  int index = find_user(username);
  if(index < 0){
      copy("User not found!",(char*)server_output, BUFFER_LENGTH);
      return;
  }
  
  if(eq(user_list[index].pw,pw)){
    copy(username, current_user, MAX_NAME_LEN);
    logged_in = true;
    copy("Logged in.",(char*)server_output, BUFFER_LENGTH);
    return;
  }

  copy("Wrong password!",(char*)server_output, BUFFER_LENGTH);

}

//  ============================
//  File Management Functions
//  ============================

int find_file(char* f){

  for(int i = 0; i < next_file; i++){
      if(eq(file_list[i].name, f)){
        return i;
      }
  }
  
  return -1;

}


void print_file(char* filename){
  
  int index = find_file(filename);
  if(index < 0){
      copy("File not found!",(char*)server_output, BUFFER_LENGTH);
      return;
  }
  
  int user_index = find_user(file_list[index].owner);

  if(!logged_in){
    copy("Not logged in! Log in to print files!",(char*)server_output, BUFFER_LENGTH);
    return;
  }

  if(logged_in && eq(user_list[user_index].name, current_user)){
    copy(file_list[index].content,(char*)server_output, BUFFER_LENGTH);
    return;
  }

  copy("You are not the owner of the file. Access denied!",(char*)server_output, BUFFER_LENGTH);  
}

void create_file(char* filename, char* content){

  if(!logged_in){
    copy("Not logged in! Log in to create files!",(char*)server_output, BUFFER_LENGTH);
    return;
  }
  
  if(!filename || filename[0] == '\0'){
    copy("Empty filename given!",(char*)server_output, BUFFER_LENGTH);
    return;
  }
  
  // Check if file already exists
  if(find_file(filename) >= 0){
    copy("File already exists! Please choose a new filename!",(char*)server_output, BUFFER_LENGTH);
    return;
  }
  
  if(!content || content[0] == '\0'){
    copy("Empty content given!",(char*)server_output, BUFFER_LENGTH);
    return;
  }
  
  if(next_file == MAX_FILE_AMOUNT){
    copy("Max. amount of files reached!",(char*)server_output, BUFFER_LENGTH);
    return;
  }

  copy(filename, file_list[next_file].name, MAX_NAME_LEN);
  copy(content, file_list[next_file].content, MAX_CONTENT_LEN);
  copy(current_user, file_list[next_file].owner, MAX_NAME_LEN);
  next_file += 1;

  copy("File created!",(char*)server_output, BUFFER_LENGTH);

}

//  ============================
//  Microkit Functions
//  ============================

void init(void) {
    microkit_dbg_puts("SERVER: starting\n");
}

void notified(microkit_channel channel){
    switch(channel){
    
      case CLIENT_CHANNEL_ID:

        int type = ((int*)request_type)[0];
        switch(type){
        
          // Create User
          case 1:
            create_user((char*)request_first_parameter,(char*)request_second_parameter);
            break;
            
          // Login
          case 2:
            login((char*)request_first_parameter,(char*)request_second_parameter);
            break;
        
          // Logout
          case 3:
            logout();
            break;
        
          // Create File
          case 4:
            create_file((char*)request_first_parameter,(char*)request_second_parameter);
            break;
        
          // Print File
          case 5:
            print_file((char*)request_first_parameter);
            break;
        
          default:
            copy("Invalid Request Type.",(char*)server_output, BUFFER_LENGTH);
            break;
        
        }

        
        microkit_notify(CLIENT_CHANNEL_ID);
        break;
    }
}
