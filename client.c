#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <openssl/md5.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <signal.h>
#define PORT 8080
#define HASH_LEN 34
#define KEY_LENGTH  2048
#define PUB_EXP     65537

// Globals to store everything.
char name[100]={0};
char pass[100]={0};
char hash[HASH_LEN]={0};
int sock = 0;
int valread;
RSA * keypair;

void initialize(){

    // No buffering
    setvbuf(stdin,NULL,_IONBF,0);
    setvbuf(stdout,NULL,_IONBF,0);
    setvbuf(stderr,NULL,_IONBF,0);
    return;
}

char *str2md5(const char *str, int length) {
    // Function to hash a string.
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char *out = (char*)malloc(33);

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, str, 512);
        } else {
            MD5_Update(&c, str, length);
        }
        length -= 512;
        str += 512;
    }

    MD5_Final(digest, &c);

    for (n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
    }

    return out;
}


void getPassword(){

    printf("Enter password : ");
    scanf("%s",pass);

    // We don't want the password to be on screen
    // so replace with *
    printf("\033[A\r");
    fflush(stdout);
    printf("Enter password : ");
    for(int i=0;i<strlen(pass);i++){
        printf("*");
    }
    printf("\n");


    // Password should be of length 8+
    while(1){
        if(strlen(pass)<8){
            printf("Password must be atleast 8 character!\nRe-Enter password : ");
            memset(pass,'\0',100);
            scanf("%s",pass);
            printf("\033[A\r");
            fflush(stdout);
            printf("Re-Enter password : ");
            for(int i=0;i<strlen(pass);i++){
                printf("*");
            }
            printf("\n");
            continue;
        }
        break;
    }

    // Hash of password.
    char * hashed = str2md5(pass,strlen(pass));
    strncpy(hash,hashed,HASH_LEN-1);
    hash[HASH_LEN-1]='\0';

    // User entered password nulled out. No password is saved in the program.
    // From here onwards, hash will be used to authenticate.
    memset(pass,'\0',100);
}
int valread;

void fatal(char *s){
    // errExit
    printf("Error: %s",s);
    exit(0);
}

unsigned int getInt(){
    // Just a function to get int.
    char s[10];
    int ret = read(0,s,9);
    if(ret<0){
        fatal("read");
    }
    s[ret-1]='\0';
    return atoi(s);
}


unsigned int mainMenu(){
    puts("----------------------------");
    puts("|1. Login                  |");
    puts("|2. Register               |");
    puts("|3. Exit                   |");
    puts("---------------------------");
    printf(">> ");
    return getInt();
}

void openChat(){
    char msg[100];
    pid_t id=0;

    memset(msg,'\0',100);

    id = fork();
    // For receiving messages
    if(!id){
        while(1){
            read(sock, msg, 100);
            if(strlen(msg)>0){
                // Clearing prompt on screen
                printf("\n");
                printf("\033[A\r");
                for(int i=0;i<100;i++)
                    printf(" ");
                printf("\n");
                printf("\033[A\r");
                printf("Message from server: %s\n",msg);
                memset(msg,'\0',100);
                printf("Enter message: ");
            }
        }
    }

    // Parent sends messages
    while(1){
        memset(msg,'\0',100);
        printf("Enter message: ");
        scanf("%100s",msg);

        send(sock, msg, strlen(msg),0);
        // killing process
        if(strncmp(msg,"quit",4)==0){
            kill(id,SIGTERM);
            exit(0);
        }
    }
}

void login(){
    // Enter username and password.
enterUsername:
    printf("Enter username : ");
    scanf("%s",name);
    send(sock , name , strlen(name) , 0 );
    char res[50];
    memset(res, '\0',50);
    read(sock, res, 49);

    // Username should exist in db.
    if(strncmp(res,"Not Found",9)==0){
        printf("Username not found.\n");
        goto enterUsername;
    }
enterPassword:
    getPassword();
    memset(res, '\0',50);
    send(sock , hash , strlen(hash) , 0 );
    read(sock, res, 49);

    // Password should match with password in db.
    if(strncmp(res,"Failed",6)==0){
        printf("Username and password doesn't match.\n");
        goto enterPassword;
    }
    puts("Type quit to terminate the connection.");
    openChat();
    return;
}

void newUser(){
    // Enter username and password
username:
    printf("Enter username : ");
    scanf("%s",name);
    send(sock , name , strlen(name) , 0 );
    char res[50];
    memset(res, '\0',50);
    read(sock, res, 49);

    // Two users with same username can't exist.
    if(strncmp(res,"Found",5)==0){
        printf("Username taken. Please choose another one.\n");
        goto username;
    }
    // No need to check anything in password.
    getPassword();
    memset(res, '\0',50);
    send(sock , hash , strlen(hash) , 0 );
    return;
}


int main(int argc, char const *argv[])
{
    initialize();
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Option provided by user.
    int option = mainMenu();
    char optBuf[5];
    sprintf(optBuf,"%d",option);

    // Option send to server so server can act accordingly.
    send(sock , optBuf, strlen(optBuf) , 0 );

    // Do according to option.
    switch(option){
        case 1: login();
                break;
        case 2: newUser();
                break;
        default: exit(0);
    }

    close(sock);
    return 0;
}
