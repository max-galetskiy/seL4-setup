#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>

#define INVALID_CHAR (-1)
#define UART_CHANNEL_ID 0
#define SERVER_CHANNEL_ID 2

#define BUFFER_LENGTH 4096

// RISC-V UART REGISTER OFFSETS
#define UART_RHR 0x00
#define UART_IER 0x01
#define UART_FCR 0x02
#define UART_LCR 0x03
#define UART_LSR 0x05

// RISC-V UART BITMASKS
#define NS16550A_LSR_THRE (1 << 5)
#define NS16550A_LSR_DR (1 << 0)

#define REG_PTR(base, offset) ((volatile uint8_t *)((base) + (offset)))

//  ============================
//  Microkit Variables
//  ============================

uintptr_t request_type;
uintptr_t request_first_parameter;
uintptr_t request_second_parameter;
uintptr_t server_output;

uintptr_t uart_base_vaddr;

//  ============================
//  IO/Buffer Variables
//  ============================

char input_buffer[BUFFER_LENGTH];

enum IO_STATE {READING_CHOICE, READING_FIRST_PARAM, READING_SECOND_PARAM};
enum IO_STATE current_state = READING_CHOICE;

enum REQUEST_TYPE {NONE = 0, CREATE_USER, LOGIN, LOGOUT, CREATE_FILE, PRINT_FILE};
enum REQUEST_TYPE current_request = NONE;

int buffer_index = 0;

//  ============================
//  UART Functions
//  ============================

void uart_init() {
    *REG_PTR(uart_base_vaddr, UART_FCR) = 0x07;
    *REG_PTR(uart_base_vaddr, UART_IER) = 0x01;
}

int uart_get_char() {
    int ch = INVALID_CHAR;

    if ((*REG_PTR(uart_base_vaddr, UART_LSR) & NS16550A_LSR_DR) != 0) {
        ch = *REG_PTR(uart_base_vaddr, UART_RHR) & 0xFF;
    }

    // In RISC-V, we often have "\r\n" to end a line. We convert everything to \n
    switch (ch) {
    case '\r':
        ch = '\n';
        break;
    case 8:
    case 127:
        ch = 0x7f;
        break;
    }
    return ch;
}

void uart_put_char(int ch) {
    while ((*REG_PTR(uart_base_vaddr, UART_LSR) & NS16550A_LSR_THRE) == 0);

    *REG_PTR(uart_base_vaddr, UART_RHR) = ch;
    if (ch == '\n') {
        uart_put_char('\r');
    }
}

void uart_handle_irq() {
    return;
    // Note: Placeholder function. Previously needed because of ARM architecture where you need to set a delete-register
    //*REG_PTR(uart_base_vaddr, UARTICR) = 0x7f0;
}

void uart_put_str(char *str) {
    while (*str) {
        uart_put_char(*str);
        str++;
    }
}

//  ============================
//  Utility Functions
//  ============================
void copy(char* src, char* dest, int max_len){

  int i = 0;
  while(src[i] != '\0' && i < (max_len-1)){
    dest[i] = src[i];
    i++;
  }
  
  dest[i] = '\0';  
}

//  ============================
//  Request Functions
//  ============================

void print_menu(){
    uart_put_str("Options: \n");
    uart_put_str("1. Create User\n");
    uart_put_str("2. Login\n");
    uart_put_str("3. Logout\n");
    uart_put_str("4. Create File\n");
    uart_put_str("5. Print File\n");
}

void process_choice(){
    
    // There is exactly one char in the buffer
    if(buffer_index == 1){
      switch(input_buffer[0]){
        case '1':
          current_request = CREATE_USER;
          uart_put_str("Enter a username:\n");
          current_state = READING_FIRST_PARAM;
          return;
        case '2':
          current_request = LOGIN;
          uart_put_str("Enter a username:\n");
          current_state = READING_FIRST_PARAM;
          return;
        case '3':
          current_request = LOGOUT;
          
          // Send Request
          ((int*)request_type)[0] = 3;
          microkit_notify(SERVER_CHANNEL_ID);
          
          current_request = NONE;
          current_state = READING_CHOICE;
          return;
        case '4':
          current_request = CREATE_FILE;
          uart_put_str("Enter a filename:\n");
          current_state = READING_FIRST_PARAM;
          return;
        case '5':
          current_request = PRINT_FILE;
          uart_put_str("Enter a filename:\n");
          current_state = READING_FIRST_PARAM;
          return;
      }
    }
    
    uart_put_str("Invalid Menu Choice!\n");
}

void process_first_param(){

    switch(current_request){
      case CREATE_USER:
      case LOGIN:
        copy(input_buffer, (char*)request_first_parameter, BUFFER_LENGTH);
        uart_put_str("Enter a password:\n");
        current_state = READING_SECOND_PARAM;
        return;
      case CREATE_FILE:
        copy(input_buffer, (char*)request_first_parameter, BUFFER_LENGTH);
        uart_put_str("Enter the content of your file:\n");
        current_state = READING_SECOND_PARAM;
        return;
      case PRINT_FILE:
        copy(input_buffer, (char*)request_first_parameter, BUFFER_LENGTH);

        // Send Request
        ((int*)request_type)[0] = 5;
        microkit_notify(SERVER_CHANNEL_ID);
      
        current_request = NONE;
        current_state = READING_CHOICE;
        return;
      default:
        uart_put_str("Error: Failed to process first parameter!\n");
        current_request = NONE;
        current_state = READING_CHOICE;
        return;      
    }
}

void process_second_param(){

    switch(current_request){
      case CREATE_USER:
        copy(input_buffer,(char*)request_second_parameter, BUFFER_LENGTH);
        
        // Send Request
        ((int*)request_type)[0] = 1;
        microkit_notify(SERVER_CHANNEL_ID);
        
        current_state = READING_CHOICE;
        return;      
      case LOGIN:
        copy(input_buffer,(char*)request_second_parameter, BUFFER_LENGTH);

        // Send Request
        ((int*)request_type)[0] = 2;
        microkit_notify(SERVER_CHANNEL_ID);
          
        current_state = READING_CHOICE;        
        return;
      case CREATE_FILE:
        copy(input_buffer,(char*)request_second_parameter, BUFFER_LENGTH);

        ((int*)request_type)[0] = 4;
        microkit_notify(SERVER_CHANNEL_ID);
        
        current_state = READING_CHOICE;        
        return;
      default:
        uart_put_str("Error: Failed to process second parameter!\n");
        current_request = NONE;
        current_state = READING_CHOICE;
        return;      
    }
}

//  ============================
//  Microkit Functions
//  ============================

void init(void) {
    uart_init();
    microkit_dbg_puts("CLIENT: starting\n");
    print_menu();
}

void notified(microkit_channel channel){
    switch(channel){
        case UART_CHANNEL_ID:

          // Handle the interrupt request and acknowledge the IRQ
          uart_handle_irq();
          microkit_irq_ack(channel);

          // Read current char and print it to the user's screen
          int chr = uart_get_char();
          if(chr == INVALID_CHAR){
            break;
          }
          uart_put_char(chr);
  
          // Backspace
          if (chr == 0x7f) {
            if(buffer_index > 0){
              buffer_index--;
              uart_put_char('\b');
              uart_put_char(' ');
              uart_put_char('\b');
            }
          }
          
          // \n reached ==> Now we have the full line and can process it depending on where we are in the current IO_STATE
          else if(chr == '\n'){
            input_buffer[buffer_index] = '\0';
  
            if(buffer_index > 0){
              switch(current_state){
                case READING_CHOICE:
                  process_choice();
                  break;
                case READING_FIRST_PARAM:
                  process_first_param();
                  break;
                case READING_SECOND_PARAM:
                  process_second_param();
                  break;
              }
              
            buffer_index = 0;

            }
          }
          
          // normal char
          else if (buffer_index < BUFFER_LENGTH - 1){
            input_buffer[buffer_index] = chr;
            buffer_index++;
          }
          
          break;
        
        case SERVER_CHANNEL_ID:
          
          uart_put_str("Server Response: ");
          uart_put_str((char*)server_output);
          uart_put_str("\n");
          print_menu();
          break;
    }
}

