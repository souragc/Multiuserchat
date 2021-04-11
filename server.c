#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#define PORT 8080
#define HASH_LEN 34

pthread_mutex_t mutex;


int clientCount = 0;

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
    printf("%s\n",luser);
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

void newUser(int new_socket){
    int valread=0;
username:
    memset(user,'\x00',100);
    valread = read( new_socket , user, 100);

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
    fileWrite();
    printf("register successful");
}

void login(int new_socket){
    int valread=0;
enterUsername: 
    memset(user,'\x00',100);
    valread = read( new_socket , user, 100);

    // Check is username exist in db.
    if(checkUser()){
        send(new_socket,"Found",5,0);
    }
    else{
        send(new_socket,"Not Found",9,0);
        goto enterUsername;
    }
enterPassword:
    valread = read(new_socket, pass, 100);

    // Password is checked with the db.
    if(!checkPass()){
        send(new_socket,"Failed", 6,0);
        goto enterPassword;
    }
    else
        send(new_socket,"Success", 7,0);
    printf("Login success");
}


int main(int argc, char const *argv[])
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // No buffering
    initialize();

    // Mutex used to avoid race in file
    pthread_mutex_init(&mutex, NULL);

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

    // Multiple clients can connect
    while(1){
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                        (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        clientCount++;
        // Sperate child for each client
        if(!fork()){
            int found = 0;
            int clienNumber = clientCount;
            char opt[2]={0};

            // Client sends the option selected
            valread = read( new_socket , opt, 2);
            int option = atoi(opt);

            // Do according to the option
            switch(option){
                case 1: login(new_socket);
                        break;
                case 2: newUser(new_socket);
                        break;
                default: exit(0);
            }

            close(new_socket);

        }
    }
    return 0;
}
