#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 43456

void initial_setup(){
    if(open("login.dat", O_RDONLY) == -1){
        printf("Setting up Account database\n");
        creat("login.dat", 0644);
        int fd = open("login.dat", O_RDWR);
        struct Account admin;
        strcpy(admin.username, "admin");
        strcpy(admin.password, "12345");
        admin.type = 3;
        admin.session = 0;
        strcpy(admin.status, "ACTIVE");
        write(fd, &admin, sizeof(admin));
        close(fd);
    }
    if(open("train.dat", O_RDONLY) == -1){
        printf("Setting up Trains database\n");
        creat("train.dat", 0644);
    }
    if(open("booking.dat", O_RDONLY) == -1){
        printf("Setting up Booking database\n");
        creat("booking.dat", 0644);
    }
    if(open("bid.dat", O_RDONLY) == -1){
        printf("Setting up Booking Number database\n");
        creat("bid.dat", 0644);
        int fd = open("bid.dat", O_RDWR);
        int bid = 1;
        write(fd, &bid, sizeof(bid));
        close(fd);
    }
}