#include<stdio.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<netinet/in.h>
#include<string.h>
#include <arpa/inet.h>
#include<signal.h>

#define PORT 43456

void signal_handler(int signal_no){
    signal(SIGINT, signal_handler);
    printf("\nLog Out Properly from the Server\n");
    fflush(stdout);
}

int main(int argc, char const *argv[])
{
    struct sockaddr_in server;
    int sock_desc;
    char output[1024];
    char message[1024];
    char input[1024];
    signal(SIGINT, signal_handler);
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);
    connect(sock_desc, (void *)(&server), sizeof(server));
    write(sock_desc, "Connected", 10);
    while(1){
        read(sock_desc, output, 1024);
        write(sock_desc, "ACK", 4);
        if(strcmp("RW", output) == 0){
            read(sock_desc, message, sizeof(message));
            printf("%s", message);
            scanf("%s", input);
            write(sock_desc, input, sizeof(input));
        }
        else if(strcmp("R", output) == 0){
            read(sock_desc, message, sizeof(message));
            write(sock_desc, "ACK", 4);
            printf("%s\n", message);

        }
        else if(strcmp("CLOSE", output) == 0){
            shutdown(sock_desc, SHUT_WR);
            printf("Connection Closed, Good Bye\n");
            break;
        }
        memset(message, 0, sizeof(message));
        memset(output, 0, sizeof(output));
    }
    return 0;
}
