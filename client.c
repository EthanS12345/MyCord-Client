/*#include <stdbool.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>
typedef enum MessageType {//Enum with message types for better reference
	LOGIN = 0,
	LOGOUT = 1,
	MESSAGE_SEND = 2,

	MESSAGE_RECV = 10,
	DISCONNECT = 12,
	SYSTEM = 13,
} message_type_t;

typedef struct __attribute__((packed)) Message {//Struct that contains message information
	uint32_t message_type;
	uint32_t timestamp;
	char username[32];
	char message[1024];
} message_t;
typedef struct Settings {//Struct that stores information about server and settings
    struct sockaddr_in server;
    bool quiet;
    int socket_fd;
    bool running;
    char username[32];
} settings_t;

bool disconnect = false;//Variable that keeps  track of whether a disconnect message has been received
bool socket_closed = false;//Variable that tells certain statements that the socket has been closed on purpose aka Signals/EOF
bool signal_received = false;//Variable that tells if a signal like SIGINT or SIGTERM has been received

static char* COLOR_RED = "\033[31m";
static char* COLOR_GRAY = "\033[90m";
static char* COLOR_RESET = "\033[0m";
static settings_t settings = {0};

//settings_t* settings = malloc(sizeof(settings_t));//Dynamically allocate the settings struct so other threads can access it

int process_args(int argc, char *argv[], settings_t* settings) {
	bool ip_or_domain = false; //Boolean variable that keeps track of whether an ip or domain flag has already been parsed and consumed
	for(int i = 1; i < argc; i++){//Loop to parse arguments starting at 1 since argv[0] is file name/path
		if(strncmp(argv[i], "--help", 7) == 0 || strncmp(argv[i], "--h", 7) == 0){//Check if current argument is the help flag
			printf("HELP MENU\n");//If so print help message (Temporary) and exit properly
			exit(0);
		}
		else if(strncmp(argv[i], "--port", 7) == 0 || strncmp(argv[i], "--PORT", 7) == 0){
			if (++i >= argc) {//Checks if the input flag exists by checking if the file name after the flag is within the argument count
                                fprintf(stderr, "Error: Missing Port filename.\n");//If not prints out and error and exits
				exit(1);
                                }
			int port = atoi(argv[i]);//Converst the string to an int for the port
			settings -> server.sin_port = htons(port);//Sets the port in the server struct to the next argument and uses htons for portability
		}
		else if(strncmp(argv[i], "--ip", 7) == 0 || strncmp(argv[i], "--IP", 7) == 0){//Checks for the ip flag in the arguments
                        if (++i >= argc) {//Checks if the input flag exists by checking if the file name after the flag is within the argument count
                                fprintf(stderr, "Error: Missing IP addres.\n");//If not prints out and error and exits
				exit(1);
                                }
			if (ip_or_domain == false){//Checks if ip or domain has already been set
				ip_or_domain = true;
				int set_ip = inet_pton(AF_INET, argv[i], &(settings->server.sin_addr));//Uses inet_aton to convert ascii string into ip address and set that value into settings struct
				switch(set_ip){//Switch statement that prints why the IP address was not set properly
					case -1:
						perror("Error: IP address was not set properly (system)\n");
					case 0:
						perror("Error: IP address was not set properly (parse)\n");

				}
			}
			else{//if it has been set already print an error message and exit
                                fprintf(stderr, "Error: Domain and IP flags present\n");
				exit(1);
                        }
		}
		else if(strncmp(argv[i], "--domain", 10) == 0 || strncmp(argv[i], "--DOMAIN", 10) == 0){//Checks for domain flags in arguments
                        if (++i >= argc) {//Checks if the input flag exists by checking if the file name after the flag is within the argument count
                                fprintf(stderr, "Error: Missing Domain.\n");//If not prints out and error and exits
				exit(1);
                                }
                        if (ip_or_domain == false){//Checks if the ip or domain flag has already been parsed and if not
                                ip_or_domain = true;//Sets the ip or domain flag to true since domain has ben parsed
				struct hostent* he = gethostbyname(argv[i]);//Creates a struct to receive the information about the domain name
				if(he == NULL){//Insures that the struct received is not null
					fprintf(stderr, "Gethostbyname returned a null pointer\n");//If it is null print error message and exit
					fprintf(stderr, "Error is %s\n", strerror(errno));
					exit(1);
				}
				if(memcpy(&(settings->server.sin_addr), he->h_addr_list[0], he->h_length) == NULL){//Copys the first address of the domain to the server struct
					fprintf(stderr, "Copy of IP address to structure failed \n");//Prints an error if the memcpy fails
					fprintf(stderr, "Error is %s\n", strerror(errno));
					exit(1);
				}

                        }
			else{//if it has been set already print an error message and exit
				fprintf(stderr, "Error: Domain and IP flags present\n");
				exit(1);
			}
                }
		else if(strncmp(argv[i], "--quiet", 8) == 0 || strncmp(argv[i], "--q", 8) == 0){//Check if current argument is the quiet flag
                        settings -> quiet = true;
			printf("Quiet flag is present\n");
			//Set quiet to true which means no alerts or highlighting
                }
		else{//If none of those conditions are true it means an unknown flag or argument was found so error and exit
			fprintf(stderr, "Unknown flag discovered\n");
			exit(1);
		}
	}
	return 0;//Return 0 if parsing was a success
}

int get_username(settings_t* settings) {
    FILE* fp = popen("whoami", "r");//Open pipe to command whoami to get username
    if (fp == NULL){//Checks if the pipe opened and if it didn't exit and print error
    	fprintf(stderr, "Could not open file whoami\n");
	exit(1);
    }
    if(fgets(settings->username, sizeof(settings->username), fp) == NULL){//Use fgets to read out the username to the username array
	fprintf(stderr, "Could not read username\n");//If the return is NULL that means it couldn't read properly so error message and exit
	exit(1);
    }
    if(pclose(fp) != 0){//Close the pipe and check that the close worked
    	fprintf(stderr, "Could not close file whoami\n");
	exit(1);
    }
    if (settings -> username == NULL){//Check if the username read is empty
    	fprintf(stderr, "Username was empty\n");//Error if it is and exit
	exit(1);
    }
    int len = strlen(settings -> username);//Find the length of the username
    if( settings -> username[len-1] == '\n' ){//Check that the last character in the array is the newline char
    	settings -> username[len-1] = 0;//Remove it by changing it to NULL terminator
    }
    for(int i = 0; i < strnlen(settings -> username, 32); i++){//Check that the username is an ascceptable ascii character list
    	if(settings -> username[i] < 48 || settings -> username[i] > 122){
		fprintf(stderr, "One of characters in username was not within acceptable ASCII range\n");
		exit(1);
		}
	}
    return 0;//Returns success if the username was correct and received correctly
}
void send_logout(){//Function that sends the logout message to server
    message_t logout_message;//Creates the instance for logout message
    memset(&logout_message, 0, sizeof(logout_message));//Initialize all memory to 0 for logout message
    logout_message.message_type = htonl(LOGOUT);//Sets the type to logout/1
    if(write(settings.socket_fd, &logout_message, sizeof(logout_message)) != sizeof(logout_message)){//Checks that the write wrote the correct number of bytes and if not error
        perror("Error: Failed to write logout message to server\n");//If the write fails error and exit
        exit(1);
    }
    settings.running = false;//Changes the running variable that controls both recv and sending loops to false so that they exit next time condition is checked
    close(settings.socket_fd);//Closes the socket file descriptor to interrupt the read() call
}

void handle_signal(int signal) {//Signal handling function
    if(!disconnect){//Checks if the disconnect variable is false, if it is false that means you should send logout and if not you shouldn't
    	send_logout();//Calls send logout which sends logout message
    	signal_received = true;
    }
    else{//Disconnect is true which means you should not send any more information
    	settings.running = false;//Running is false to break loops
	close(settings.socket_fd);//Close file descriptor to break read() 
	printf("Disconnect message was received and socket was closed\n");//Print message that Disconnect was received
    	exit(0);//Exit the process properly since disconnect was received
    }
}

ssize_t perform_full_read(void *buf, size_t n) {//Function to perform a full read 
	ssize_t total_read = 0;//Holds the number of bytes read
	while(total_read < n){//Loop that iteratres until the total bytes is equal to the number of bytes requested
		ssize_t bytes_received = read(settings.socket_fd, buf+total_read , n-total_read);//Reads out the struct from stdin
		if (bytes_received < 0){//Ensure the read did not fail
			if(errno == EINTR) continue;//If it was interrupted continue the read
			if(errno == EBADF){//If the file descriptor is bad that means it was closed
				printf("File descriptor was closed\n");//Prints that the file descriptor was closed and sets the var accordingly
				socket_closed = true;	
				break;//break the loop since you don't want to continue reading
			}
			else{//Otherwise the error is unknown or not meant to be handles print error messages and exit
				fprintf(stderr, "Error is %s\n", strerror(errno));
				fprintf(stderr, "Error is %d\n", errno);
				break;
			}
			
		}
		total_read += bytes_received;//Iteratre the total read by how many bytes this specific instance read
	}
        return total_read;//Return the number of bytes read
}

void* receive_messages_thread(void* arg) {
    // while some condition(s) are true
        // read message from the server (ensure no short reads)
        // check the message type
            // for message types, print the message and do highlight parsing (if not quiet)
            // for system types, print the message in gray with username SYSTEM
            // for disconnect types, print the reason in red with username DISCONNECT and exit
            // for anything else, print an error
   message_t message_received;//Create a structure that holds information about received messages
   while (settings.running){//Run while running is false aka no disconnect message received
	ssize_t bytes_read = perform_full_read(&message_received, sizeof(message_received));//Reads the full number of bytes
	if(socket_closed){//If the global socket_closed variable is true that means the socket was closed
		settings.running = false;//Change the loop condition
		break;//break out of the loop
	}
	if(bytes_read < 0){//Handles scenario where read call didn't work and it wasn't because it was interrupted or file descriptor was closed
		fprintf(stderr, "Error: No bytes read, read call failed\n");//Error
                settings.running = false;//Change loop condition
                break;//Break out of loop
       	}
	else if (bytes_read < sizeof(message_received)){//Probably unnecessary since the full read should always read the right amount unless an error occurs
		fprintf(stderr, "Error: Short read occurred\n");//Error message
		settings.running = false;//Change loop condition
		break;//Break out of loop
	}	
	uint32_t type = ntohl(message_received.message_type);//Converts both int types from network byte order to host byte order
	uint32_t timestamp = ntohl(message_received.timestamp);
	message_received.message_type = type;
	message_received.timestamp = timestamp;
	if (type == MESSAGE_RECV){//Checks if the message type is MESSAGE_RECV aka 10
		char time_buff[100];//Character array that holds the time in the correct format
		time_t time = (time_t) message_received.timestamp;//Converts the time sent into the right type
		struct tm* converted_time = localtime(&time);//Convers the time into the structure that will be converted
		strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %H:%M:%S", converted_time);//Converts the time structure into the correct string format
		printf("[%s] %s: ", time_buff, message_received.username);//Prints out the time and username in the right format
		char* msg_pointer = message_received.message;//Pointer to the first character of the message that was sent
		message_received.message[1023] == '\0';//Null terminates the sent message just in case
		while(*msg_pointer != '\0'){//Iteratres until it reaches the null terminator and all messages should always be null terminated
			if(*msg_pointer == '@'){//Checks if at the current index there is an @ symbol which indicates an @ that should be highlighted
				msg_pointer += 1;//Consumes the @ character
				if(strncmp(msg_pointer, settings.username, strlen(message_received.username)) == 0){//Checks if the character's following the @ are the same as the username
					if(settings.quiet == false){//Checks if quiet is off and if so highlights and pings format
						printf("\a%s@%s%s", COLOR_RED, settings.username, COLOR_RESET);
						}
					else{//Otherwise just prints out the message
						printf("@%s", settings.username);
					}
					msg_pointer += strlen(settings.username);//Consumes the entire username since it matched
				}
				else{//If the username does not match just print out the @ symbol that was consumes
					printf("@");
				}
			}
			else{//Otherwise just prints out the character
				printf("%c", *msg_pointer);
				msg_pointer += 1;
			}
		}
	       printf("\n");//Adds newline character to the end of each message
	}
	else if (type == DISCONNECT){//Checks if the mssage type is DISCONNECT and prints the message accordingly
		printf("%s[DISCONNECT] %s %s\n", COLOR_RED, message_received.message, COLOR_RESET);//Prints message and ends the while loop since disconnect was detected
		settings.running = false;//Change loop condition
		disconnect = true;//Change variable that means a disconnect message was received
		close(settings.socket_fd);//Closes the file descriptor
		raise(SIGINT);//Raises a signal to exit the process cleanly
		break;//Break out of loop
	}
	else if (type == SYSTEM){//Checks if it was a system message and prints out accordingly
		printf("%s[SYSTEM] %s%s\n", COLOR_GRAY, message_received.message, COLOR_RESET);
	}
	else{//Message type is not familiar exit and print error message
		fprintf(stderr, "Error: Unfamiliar message type detected\n");
		settings.running = false;
		break;
	}



   }
   return NULL;//Return NULL if the thread ran smoothly
}

void set_defaults(settings_t* settings){
	settings -> quiet = false; //Default to false
	settings -> running = true; //Running default is true
	settings -> server.sin_family = AF_INET;//Defaults type of family to IPV4
	settings -> server.sin_port = htons(8080); //Default port
	settings -> socket_fd = 0;//Default file descriptor
	if(inet_pton(AF_INET, "127.0.0.1", &settings -> server.sin_addr) != 1){//Sets teh default IP address and prints error messages if it fails
		fprintf(stderr, "Error: Trouble setting default IP address\n");
		exit(1);
	}
}
bool is_valid_message(char* line, size_t len){//Checks if a message is valid
	if(len == 0 || len > 1023){//Checks if length is too long or empty
		fprintf(stderr, "Error: Message was invalid due to too few or too many characters\n");
		return false;
	}
	for (int i = 0; i < strnlen(line, 1024); i++){//Iteratres through every character in the message
                if(isprint(line[i]) == 0){//Checks that each character is printable and prints error message and returns false if not
                        fprintf(stderr, "Error: Message was invalid due to non printable ascii character included\n");
                        return false;
                }
            }
	return true;//Returns true if the message was valid
}

int main(int argc, char *argv[]) {
    settings_t* settings_pt = &settings;
    set_defaults(settings_pt);//Sets the defaults for the settings struct

    // setup sigactions (ill-advised to use signal for this project, use sigaction with default (0) flags instead)
    struct sigaction sa;//Creates a struct for signal ahndling
    memset(&sa, 0, sizeof(sa));//Initializes all memory to 0
    sa.sa_handler = handle_signal;//Sets the function that will be called
    sigaction(SIGINT, &sa, NULL);//Signal handlers for SIGINT and SIGTERM
    sigaction(SIGTERM, &sa, NULL);
    
    // parse arguments
    process_args(argc, argv, settings_pt);//Processes arguemnts in the cli
    
    // get username
    get_username(settings_pt);//Calls get_username function to get and pass username
    
    // create socket
    settings_pt -> socket_fd = socket(AF_INET, SOCK_STREAM, 0);//Create the socket and pass the file descriptor to the struct
    if (settings_pt -> socket_fd < 0){//Check for an error creating the fd
    	fprintf(stderr, "Error on socket creation [%s]\n", strerror(errno));//Error message
	exit(1);//Exit process due to error
    }
    // connect to server
    int connection = connect(settings_pt -> socket_fd, (struct sockaddr*) &(settings_pt -> server), sizeof(settings_pt -> server));//Connects to the server
    if (connection != 0){//Checks that the connetion was a success and if not prints why 
	fprintf(stderr, "Error is %s\n", strerror(errno));//Prints the error
	close(settings_pt -> socket_fd);//Close the file descriptor
	exit(1);//Exits the process
    }
    //Send login message
    message_t login_message;//Creates the instance for login message
    memset(&login_message, 0, sizeof(login_message));//Initialize all memory to 0 for login message
    login_message.message_type = ntohl(LOGIN);//Sets the type to login or 0
    strncpy(login_message.username, settings_pt -> username, sizeof(settings_pt -> username));//Copies the found username from the get_username function to the message
    if(write(settings_pt -> socket_fd, &login_message, sizeof(login_message)) != sizeof(login_message)){//Checks that the write wrote the correct number of bytes and if not error
    	perror("Error: Failed to write login message to server\n");//Prints the error
	close(settings_pt -> socket_fd);//Closes the file descriptor
	exit(1);//Exit process
    }

    // create and start receive messages thread
    pthread_t recv_thread;//Create the reception thread
    if(pthread_create(&recv_thread, NULL, receive_messages_thread, NULL) != 0){//Actually create the thread
    	send_logout();//Sends logout message if receiveing thread was not created properly
	fprintf(stderr, "Error: Receiving thread was not created properly\n");
	exit(1);//Exit process
    }
    // while some condition(s) are true
        // read a line from STDIN
        // do some error checking (handle EOF, EINTR, etc.)
        // send message to the server
    while(settings.running && feof(stdin) == false){//Loop that runs so long as EOF is not encountered and the running variable is not false
	    message_t send_message;//Create the struct that contains outgoing messages
	    size_t size = sizeof(send_message.message);//Variable that holds the size of just the actual message
	    memset(&send_message, 0, sizeof(send_message));//Initializes all memory to 0
	    char* line = NULL;//Creates a variable/buffer that holds the input from stdin
	    size_t bytes_read = getline(&line, &size, stdin);//Reads out from stdin
	    if (bytes_read == -1){//Checks for an error in the read and prints out error if so
		if (errno == EINTR){//If the getline is interrupted continue
			free(line);//Free the dynamically allocated memory just in case
			line = NULL;//Set it to null as good practice
			continue;
		}
		if (feof(stdin) == true){//If EOF is encountered
			printf("EOF encountered\n");//Print a message letting user know
			free(line);//Free the memory
			line = NULL;
			break;//break out of the loop
		}
		//Otherwise it is an unknown error so print and break out of loop
	    	fprintf(stderr, "Error: Error occured with reading from stdin %s\n", strerror(errno));
		settings.running = false;//Change loop condition
		free(line);//Free and NULL pointer to heap memory
		line = NULL;
		break;
	    }
	    int length = strlen(line);//Gets the length of the message
	    if(line[length-1] == '\n'){//Checks if the last character of the message is a newline character
	    	line[length-1] = '\0';//If so changes it to a null terminator
		length -= 1;//Subtracts 1 from length since the new line was removed
	    }
	    if(!is_valid_message(line, length)){//Checks if the message is invalid
	    	free(line);//Frees memory and NULLs it
		line = NULL;
		continue;//Continues since you still want to be able to send messages
	    }
	    send_message.message_type = htonl(MESSAGE_SEND);//Changes the message type to messsage send since the message was valid
	    strncpy(send_message.message, line, sizeof(send_message.message)-1);//Copies the message from dynamically allocated buffer to the message
	    if(write(settings_pt -> socket_fd, &send_message, sizeof(message_t)) == -1){//Writes the message to the server
		free(line);//Frees and Nulls memory
		line = NULL;
 	    	fprintf(stderr, "Error: Write was unable to send message\n");//Error message that write failed
	    	break;
	    }
	    free(line);//Frees the dynamic memory allocated by getline()
	    line = NULL;//Sets it to null to prevent errors
    }
    if (!disconnect && !signal_received){//Checks if disconnect and signal received are false because if either are true the logout has already been sent and socket closed
	printf("Sending Logout since disconnect message was not received\n");//Sends the logout message
    	send_logout();
    }
    void* recv_thread_return;//Waits for the receiving thread to return
    pthread_join(recv_thread, &recv_thread_return);//Gets the return value
    if(recv_thread_return == NULL){//Checks its return
        printf("Recv_thread returned\n");//prints that it returned if return is valid
    }

    // cleanup and return
    close(settings_pt -> socket_fd);//Close the file descriptor
    settings_pt -> socket_fd = -1;//Set it to -1 as good practice
    exit(0);//Exit the proess
}*/


#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

// Packed message struct: 4 + 4 + 32 + 1024 = 1064 bytes
#pragma pack(push,1)
struct mycord_msg {
    uint32_t type;       // message type
    uint32_t ts;         // unix timestamp
    char username[32];   // null-terminated
    char message[1024];  // null-terminated
};
#pragma pack(pop)

// message types
enum {
    MT_LOGIN = 0,
    MT_LOGOUT = 1,
    MT_MESSAGE_SEND = 2,

    MT_MESSAGE_RECV = 10,
    MT_DISCONNECT = 12,
    MT_SYSTEM = 13
};

static int sockfd = -1;
static volatile sig_atomic_t running = 1;
static char *username = NULL;
static bool quiet = false;
static pthread_t recv_thread;

static void eprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

// read exactly n bytes or return -1
static ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n;
    char *p = buf;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return n - left; // EOF
        left -= r;
        p += r;
    }
    return n;
}

static ssize_t writen(int fd, const void *buf, size_t n) {
    size_t left = n;
    const char *p = buf;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= w;
        p += w;
    }
    return n;
}

static void format_and_print_user(uint32_t ts, const char *from, const char *msg) {
    time_t t = (time_t)ts;
    struct tm tm;
    localtime_r(&t, &tm);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm);

    // handle mentions: replace occurrences of @username with bell + red highlight
    if (!quiet && username) {
        size_t uname_len = strlen(username);
        const char *p = msg;
        printf("[%s] %s: ", timestr, from);
        while (*p) {
            const char *found = NULL;
            if (*p == '@') {
                if (strncmp(p+1, username, uname_len) == 0 && (p[1] != '\0')) {
                    // ensure boundary: next char after username is non-alphanumeric or end
                    const char after = p[1 + uname_len];
                    if (after == '\0' || !isalnum((unsigned char)after)) {
                        // print bell and red highlight
                        printf("\a\033[31m@%s\033[0m", username);
                        p += 1 + uname_len;
                        continue;
                    }
                }
            }
            putchar(*p);
            p++;
        }
        putchar('\n');
    } else {
        printf("[%s] %s: %s\n", timestr, from, msg);
    }
    fflush(stdout);
}

static void print_system(const char *msg) {
    printf("\033[90m[SYSTEM] %s\033[0m\n", msg);
    fflush(stdout);
}

static void print_disconnect(const char *msg) {
    printf("\033[31m[DISCONNECT] %s\033[0m\n", msg);
    fflush(stdout);
}

static void *recv_loop(void *arg) {
    (void)arg;
    struct mycord_msg m;
    while (running) {
        ssize_t got = readn(sockfd, &m, sizeof(m));
        if (got <= 0) {
            // connection closed or error
            running = 0;
            break;
        }
        uint32_t type = ntohl(m.type);
        uint32_t ts = ntohl(m.ts);
        m.username[31] = '\0';
        m.message[1023] = '\0';

        if (type == MT_MESSAGE_RECV) {
            format_and_print_user(ts, m.username, m.message);
        } else if (type == MT_SYSTEM) {
            print_system(m.message);
        } else if (type == MT_DISCONNECT) {
            print_disconnect(m.message);
            // server will close socket; stop.
            running = 0;
            break;
        } else {
            // Unknown types: ignore
        }
    }
    return NULL;
}

static void cleanup_and_exit(int status) {
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
    if (username) free(username);
    exit(status);
}

static void send_logout_and_exit(void) {
    if (sockfd != -1) {
        struct mycord_msg m;
        memset(&m, 0, sizeof(m));
        m.type = htonl(MT_LOGOUT);
        writen(sockfd, &m, sizeof(m));
        // close socket below
    }
    running = 0;
}

static void sig_handler(int signo) {
    (void)signo;
    // try to send logout
    send_logout_and_exit();
}

static bool valid_username(const char *u) {
    if (!u) return false;
    size_t n = strlen(u);
    if (n == 0 || n >= sizeof(((struct mycord_msg*)0)->username)) return false;
    for (size_t i = 0; i < n; ++i) {
        if (!isalnum((unsigned char)u[i])) return false; // require alphanumeric as requested
    }
    return true;
}

static bool valid_message(const char *s, size_t len) {
    if (len == 0 || len > 1023) return false;
    for (size_t i = 0; i < len; ++i) {
        if (!isprint((unsigned char)s[i])) return false;
        if (s[i] == '\n' || s[i] == '\r') return false;
    }
    return true;
}

int main(int argc, char **argv) {
    // defaults
    char *ip = "127.0.0.1";
    char *domain = NULL;
    int port = 8080;

    // simple arg parsing
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("usage: ./client [-h] [--port PORT] [--ip IP] [--domain DOMAIN] [--quiet]\n\n");
            printf("mycord client\n\n");
            printf("options:\n  --help                show this help message and exit\n");
            printf("  --port PORT           port to connect to (default: 8080)\n");
            printf("  --ip IP               IP to connect to (default: \"127.0.0.1\")\n");
            printf("  --domain DOMAIN       Domain name to connect to (if domain is specified, IP must not be)\n");
            printf("  --quiet               do not perform alerts or mention highlighting\n");
            return 0;
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) { eprintf("missing value for --port"); return 1; }
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) { eprintf("invalid port"); return 1; }
        } else if (strcmp(argv[i], "--ip") == 0) {
            if (i + 1 >= argc) { eprintf("missing value for --ip"); return 1; }
            ip = argv[++i];
        } else if (strcmp(argv[i], "--domain") == 0) {
            if (i + 1 >= argc) { eprintf("missing value for --domain"); return 1; }
            domain = argv[++i];
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else {
            eprintf("unknown argument: %s", argv[i]);
            return 1;
        }
    }

    if (domain && ip) {
        // if domain specified, ip must not be (per spec). We set default ip earlier; check if user explicitly passed --ip
        // Distinguish whether ip is default or explicitly passed is hard; the spec said if domain specified, IP must not be.
        // We'll error if domain != NULL and user also passed --ip explicitly. A simple heuristic: if domain set and ip != NULL and strcmp(ip, "127.0.0.1")!=0, then they passed ip.
        if (strcmp(ip, "127.0.0.1") != 0) {
            eprintf("--ip and --domain cannot both be specified");
            return 1;
        }
    }

    // Resolve username via whoami or $USER
    FILE *p = popen("whoami", "r");
    if (p) {
        char *line = NULL;
        size_t l = 0;
        ssize_t r = getline(&line, &l, p);
        if (r > 0) {
            if (line[r-1] == '\n') line[r-1] = '\0';
            username = strdup(line);
        }
        free(line);
        pclose(p);
    }
    if (!username || strlen(username) == 0) {
        char *env = getenv("USER");
        if (env && strlen(env) > 0) username = strdup(env);
    }
    if (!username) {
        eprintf("unable to determine username");
        return 1;
    }
    if (!valid_username(username)) {
        eprintf("invalid username: must be non-empty and alphanumeric");
        free(username);
        return 1;
    }

    // Resolve domain if provided
    char target_ip[INET_ADDRSTRLEN];
    if (domain) {
        struct hostent *h = gethostbyname(domain);
        if (!h) {
            eprintf("DNS resolution failed for domain: %s", domain);
            free(username);
            return 1;
        }
        struct in_addr **addr_list = (struct in_addr **)h->h_addr_list;
        if (!addr_list[0]) { eprintf("no IPv4 addresses for domain"); free(username); return 1; }
        strncpy(target_ip, inet_ntoa(*addr_list[0]), sizeof(target_ip));
        target_ip[sizeof(target_ip)-1] = '\0';
    } else {
        strncpy(target_ip, ip, sizeof(target_ip));
        target_ip[sizeof(target_ip)-1] = '\0';
    }

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { eprintf("socket: %s", strerror(errno)); free(username); return 1; }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, target_ip, &serv.sin_addr) != 1) {
        eprintf("invalid target ip: %s", target_ip);
        close(sockfd); free(username); return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        eprintf("connect: %s", strerror(errno));
        close(sockfd); free(username); return 1;
    }

    // setup signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Send LOGIN
    struct mycord_msg m;
    memset(&m, 0, sizeof(m));
    m.type = htonl(MT_LOGIN);
    strncpy(m.username, username, sizeof(m.username)-1);
    if (writen(sockfd, &m, sizeof(m)) != sizeof(m)) {
        eprintf("failed to send login: %s", strerror(errno));
        close(sockfd); free(username); return 1;
    }

    // spawn receiver thread
    if (pthread_create(&recv_thread, NULL, recv_loop, NULL) != 0) {
        eprintf("pthread_create failed");
        close(sockfd); free(username); return 1;
    }

    // main loop: read stdin and send messages
    char *line = NULL;
    size_t linecap = 0;
    while (running && !feof(stdin)) {
        ssize_t nread = getline(&line, &linecap, stdin);
        if (nread < 0) {
            // EOF or error
            break;
        }
        // strip newline(s)
        while (nread > 0 && (line[nread-1] == '\n' || line[nread-1] == '\r')) {
            line[--nread] = '\0';
        }
        if (nread == 0) {
            // ignore empty lines
            continue;
        }
        if (!valid_message(line, (size_t)nread)) {
            eprintf("invalid message: must be 1-1023 printable ASCII characters and no newlines");
            continue;
        }
        // prepare MESSAGE_SEND
        struct mycord_msg out;
        memset(&out, 0, sizeof(out));
        out.type = htonl(MT_MESSAGE_SEND);
        strncpy(out.message, line, sizeof(out.message)-1);
        if (writen(sockfd, &out, sizeof(out)) != sizeof(out)) {
            eprintf("failed to send message: %s", strerror(errno));
            running = 0;
            break;
        }
    }
    free(line);

    // EOF or termination: send LOGOUT
    send_logout_and_exit();

    // wait for recv thread to finish
    pthread_join(recv_thread, NULL);

    cleanup_and_exit(0);
    return 0;
}

