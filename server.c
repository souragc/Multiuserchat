#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#define PORT 8080
#define HASH_LEN 34

#define debug 0

// One for file writing race prevention
// Other for mem read/write race prevention.
pthread_mutex_t mutex, memMutex;



// Shared memory for children
char * ptr = 0;

// Globals for client provided user, pass.
// and also user and pass from db.
// Since each client is a different child,
// there won't be any clash here.
char user[100]={0};
char pass[100]={0};
char luser[100]={0};
char lpass[100]={0};

void initialize(){
    setvbuf(stdin,NULL,_IONBF,0);
    setvbuf(stdout,NULL,_IONBF,0);
    setvbuf(stderr,NULL,_IONBF,0);
    return;
}


int checkUser(){
    int found = 0;

    // Mutex to avoid race.
    pthread_mutex_lock(&mutex);
    FILE * fd = fopen("userpass.db","a+");
    memset(luser,'\x00',100);
    fscanf(fd,"%s %s",luser,lpass);
    while(strlen(luser)>0){

        // Takes the longer of client username and db username.
        // This is to ensure that similar usernames can exist.
        // But exact match can't exist.
        if(strncmp(luser,user,strlen(luser)>strlen(user)?strlen(luser):strlen(user))==0){
            found = 1;
            break;
        }
        memset(luser,'\x00',100);
        fscanf(fd,"%s %s",luser,lpass);
    }
    pthread_mutex_unlock(&mutex);
    return found;
}

// If new client joined or existing left, active users will be updated.
void activeUpdate(int myNum, int type){
    int i=0;
#if debug
    printf("calling update\n");
#endif
    char name[100]={0};
    if(type==1)
        // Bytes which user cannot enter into chatbox is used to distinguish between
        // normal messages and special messages
        name[0]=(char)'\x05';
    else
        name[0]=(char)'\x06';
    memcpy(name+1,user,98);
#if debug
    printf("trying %d with mynum %d\n",i,myNum);
#endif
try:
    while(1){
        pthread_mutex_lock(&memMutex);
        if(*(ptr+(200*i))==(char)'\xff'){
#if debug
            printf("Exiting update\n");
#endif
            pthread_mutex_unlock(&memMutex);
            break;
        }
        if(*(ptr+(200*i)) && i!=myNum){
            if(!*(ptr+(i*200)+1) || *(ptr+(200*i)+1)==(char)'\xff'){
                memcpy((ptr+(i*200)+1),name,strlen(name));
            }
            else{
                pthread_mutex_unlock(&memMutex);
                usleep(500);
                goto try;
            }
        }
        i++;
        pthread_mutex_unlock(&memMutex);
    }
    return;
}


int checkPass(){
    int found = 0;

    // Mutex to avoid race
    pthread_mutex_lock(&mutex);
    FILE * fd = fopen("userpass.db","a+");
    memset(luser,'\x00',100);
    memset(lpass,'\x00',100);
    fscanf(fd,"%s %s",luser,lpass);
    while(luser!=NULL){
        // Loop till we find a match for username.
        if(strncmp(luser,user,strlen(luser))==0){
            found = 1;
            break;
        }
        memset(luser,'\x00',100);
        memset(lpass,'\x00',100);
        fscanf(fd,"%s %s",luser,lpass);
    }
    if(found){

        // Check is password associated with username is same.
        if(strncmp(lpass,pass,33)==0)
            return 1;
    }
    return 0;
    pthread_mutex_unlock(&mutex);
}

void fileWrite(){

    // Mutex to avoid race
    pthread_mutex_lock(&mutex);
    FILE * fd = fopen("userpass.db","a+");
    fprintf(fd,"%s %s\n",user,pass);
    fclose(fd);
    pthread_mutex_unlock(&mutex);
}

void saveMsg(char * msg){
    pthread_mutex_lock(&mutex);
    FILE * fd = fopen("messages.db","a+");
    fputs(msg,fd);
    fclose(fd);
    pthread_mutex_unlock(&mutex);
    return;
}

void viewHistory(int new_socket){
    char msg[200]={0};
    pthread_mutex_lock(&mutex);
    FILE * fd = fopen("messages.db","a+");
    fgets(msg,200,fd);
    while(strlen(msg)>0){
        send(new_socket,msg,strlen(msg),0);
        usleep(5000);
        memset(msg,'\x00',200);
        fgets(msg,200,fd);
    }
    fclose(fd);
    pthread_mutex_unlock(&mutex);
    return;
}
void openChat(int new_socket,int myNum){
#if debug
    printf("Called openchat\n");
#endif
    char msg[200];
    pid_t id=0;
    activeUpdate(myNum, 1);
    viewHistory(new_socket);
    memset(msg,'\0',200);
    id = fork();
#if debug
    printf("called fork\n");
#endif
    // For receiving messages
    if(!id){
        while(1){
            read(new_socket, msg, 200);
            if(strlen(msg)>0){
                // Clearing prompt on screen
#if debug
                printf("Message from client: %s\n",msg);
#endif
                int count = 0;

                // When we get a message from client, we copy it to all other clients memory.
                // This will be later taken by the respective client.
#if debug
                printf("Got message locking\n");
                // killing process
                printf("%s\n",msg);
#endif
                if(strstr(msg,"quit")){
#if debug
                    printf("Client quitting\n");
#endif
                    pid_t ppid = getppid();
                    activeUpdate(myNum, 2);
                    // Set the first byte of memory to null so next client can reuse
                    // the area.
                    pthread_mutex_lock(&memMutex);
                    memset(ptr+(myNum*200),'\x00',200);
                    pthread_mutex_unlock(&memMutex);

                    kill(ppid, SIGTERM);
                    exit(0);
                }
                // Ping messages not to be saved
                if(msg[0]!='\x04')
                    saveMsg(msg);
tryAgain:
                pthread_mutex_lock(&memMutex);
                while(1){
                    // We don't want to go beyond our memory which is in use.
                    if(*(ptr+(count*200))==(char)'\xff'){
                        pthread_mutex_unlock(&memMutex);
                        break;
                    }
                    // If first byte is null that means client exited.
                    // Also we don't want to copy msg to our own memory.
#if debug
                    printf("Value at index 0 %d with count %d and myNum %d\n",(char)*(ptr+(count*200)),count,myNum);
#endif
                    if(*(ptr+(count*200)) && count!=myNum){
                        // If the data part is not null, that means the other client
                        // has not yet taken it's previoud messge.
                        if(!*(ptr+(count*200)+1) || *(ptr+(count*200)+1)==(char)'\xff'){
#if debug
                            printf("Trying to write to client number %d\n",count);
#endif
                            memcpy((ptr+(count*200)+1),msg,strlen(msg)<199?strlen(msg):198);
                        }
                        // In that case, unlock mutex and give some time for other client to take the message.
                        else{
                            pthread_mutex_unlock(&memMutex);
                            usleep(500);
                            goto tryAgain;
                        }
                    }
                    else{
                        printf("My own mem\n");
                    }
                    count++;
                    usleep(500);
                }
#if debug
                printf("Unlocking\n");
#endif
                pthread_mutex_unlock(&memMutex);
                memset(msg,'\0',200);
            }
            usleep(500);
        }
    }

    // Parent sends messages
    while(1){
        memset(msg,'\0',200);
        pthread_mutex_lock(&memMutex);
        // If we have data in our own memory, copy that to msg and send to client.
        if(*(ptr+(myNum*200)+1) && *(ptr+(myNum*200)+1)!=(char)'\xff'){
#if debug
            printf("Got message, sending to client\n");
#endif
            memcpy(msg,(ptr+(myNum*200)+1),strlen((ptr+(myNum*200)+1)));
            // Nulll out the area so next message can be put in the same place.
            memset((ptr+(myNum*200)+1),'\x00',199);
        }
        pthread_mutex_unlock(&memMutex);

        send(new_socket, msg, strlen(msg),0);
        // Don't overwhelm the client
        usleep(5000);
    }
}



void newUser(int new_socket, int myNum){
    int valread=0;
username:
    memset(user,'\x00',100);
    valread = read( new_socket , user, 100);
    if(user[0]=='\x08'){
#if debug
        printf("leaving");
#endif
        pthread_mutex_lock(&memMutex);
        memset(ptr+(myNum*200),'\x00',200);
        pthread_mutex_unlock(&memMutex);
        exit(0);
    }

    // Two users can't have the same username.
    // If username exist, client is asked to enter new username.
    if(!checkUser()){
        send(new_socket,"Not Found", 9,0);
    }
    else{
        send(new_socket,"Found", 5 ,0);
        goto username;
    }

    // Username and hash of password is written to the file.
    valread = read(new_socket, pass, 100);
    if(user[0]=='\x08'){
#if debug
        printf("leaving");
#endif
        pthread_mutex_lock(&memMutex);
        memset(ptr+(myNum*200),'\x00',200);
        pthread_mutex_unlock(&memMutex);
        exit(0);
    }
    fileWrite();
#if debug
    printf("register successful");
#endif
    openChat(new_socket, myNum);
}


void login(int new_socket, int myNum){
#if debug
    printf("Trying to login\n");
#endif
    int valread=0;
enterUsername: 
    memset(user,'\x00',100);
    valread = read( new_socket , user, 100);
    if(user[0]=='\x08'){
#if debug
        printf("leaving");
#endif
        pthread_mutex_lock(&memMutex);
        memset(ptr+(myNum*200),'\x00',200);
        pthread_mutex_unlock(&memMutex);
        exit(0);
    }

    // Check is username exist in db.
    if(checkUser()){
        send(new_socket,"Found",5,0);
#if debug
        printf("Correct username\n");
#endif
    }
    else{
        send(new_socket,"Not Found",9,0);
        goto enterUsername;
    }
#if debug
    printf("Checking password\n");
#endif
enterPassword:
    valread = read(new_socket, pass, 100);

    if(user[0]=='\x08'){
#if debug
        printf("leaving");
#endif
        pthread_mutex_lock(&memMutex);
        memset(ptr+(myNum*200),'\x00',200);
        pthread_mutex_unlock(&memMutex);
        exit(0);
    }

    // Password is checked with the db.
    if(!checkPass()){
        send(new_socket,"Failed", 6,0);
        goto enterPassword;
    }
    else
        send(new_socket,"Success", 7,0);
#if debug
    printf("Login success");
    printf("Opening chat\n");
#endif
    openChat(new_socket, myNum);
}


int main(int argc, char const *argv[])
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // No buffering
    initialize();

    // Shared memory initialization.
    // Each child gets 300 bytes
    // 1 bytes for inuse. 100 bytes for name, 199 bytes for data and 1 bytes for null
    ptr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // Initialize with \xff to help in utilizing unsued memory used by
    // previousely connected client.

#if debug
    printf("%d\n",ptr);
#endif
    memset(ptr,'\xff',0x1000);

    // Mutex used to avoid race in file
    pthread_mutex_init(&mutex, NULL);
    // Mutex to avoid mem race
    pthread_mutex_init(&memMutex, NULL);

    // Normal socket setup
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    if (bind(server_fd, (struct sockaddr *)&address, 
                sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    int clientNum = 0;

    // Multiple clients can connect
    while(1){
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                        (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        // Finding free memory
        // If a client previousely used a memory and disconnected, it will be null
        // else if it was not used, it will be \xff
        clientNum=0;
        pthread_mutex_lock(&memMutex);
        while(1){
            if(!*(ptr+(clientNum*200)) || *(ptr+(clientNum*200))==(char)'\xff')
                break;
            clientNum++;
        }
        *(ptr+(clientNum*200)) = (char) '\x01';
        memset((ptr+(clientNum*200)+1),'\x00',199);
        pthread_mutex_unlock(&memMutex);
        // Sperate child for each client
        if(!fork()){
            char opt[2]={0};

            // Client sends the option selected
            valread = read( new_socket , opt, 2);
            int option = atoi(opt);
#if debug
            printf("got message from client 0x%x\n",opt[0]);
#endif

            switch(opt[0]){
                case '\x08': 
#if debug
                            printf("client exiting\n"); 
#endif
                             // Free up the memory so next client can use it.
                             pthread_mutex_lock(&memMutex);
                             memset(ptr+(clientNum*200),'\x00',200);
                             pthread_mutex_unlock(&memMutex);
                             exit(0);
            }

            // Do according to the option
            switch(option){
                case 1: login(new_socket, clientNum);
                        break;
                case 2: newUser(new_socket, clientNum);
                        break;
                case 3: 
#if debug
                        printf("client exiting\n");
#endif
                        // Free up the memory so next client can use it.
                        pthread_mutex_lock(&memMutex);
                        memset(ptr+(clientNum*200),'\x00',200);
                        pthread_mutex_unlock(&memMutex);
                        exit(0);
                default: exit(0);
            }

            close(new_socket);

        }
    }
    return 0;
}
