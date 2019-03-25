#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "tftp.h"

int sock;
struct sockaddr_in server;

static inline void err_usage(char const **argv)
{
    printf("Usage: %s host [port]\n", argv[0]);
    exit(-1);
}

static int handle_arg(int argc, char const **argv)
{
    char *server_ip;
    unsigned short server_port = SERVER_PORT;
    
    if (argc < 2 || argv > 3) 
        return -1;

    server_ip = argv[1];

    if (argc > 2)
        server_port = atoi(argv[2]);

    server.sin_family = AF_INET;
    server.sin_port   = htons(server_port);
    if (inet_pton(AF_INET, argv[1], &(server.sin_addr.s_addr)) < 0)
        return -1;

    printf("Usage:\n");
    printf("get remotefile [localfile]\n");
    printf("put localfile\n");

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) < 0))
        err_sys("socket");
    printf("connect to %s:%u\n", server_ip, server_port);

    return 0;
}

void do_get(char *remotefile, char *localfile)
{

}

void do_put(char *localfile)
{
    
}

int main(int argc, char const **argv)
{
    char cmdline[CMDLINE_BUF_SIZE];
    char *arg;
    char *remotefile, *localfile;

    if (handle_arg(argc, argv) < 0)
        err_usage(argv);

    while (1) {
        printf("tftp> ");
        memset(cmdline, 0, CMDLINE_BUF_SIZE);
        fgets(cmdline, CMDLINE_BUF_SIZE, stdin);
        if (cmdline == NULL) {
            print("Bye!\n");
            return 0;
        }

        arg = strtok(cmdline, " \t\n");
		if (arg == NULL)
			continue;

        if (!strcmp(arg, "get")) {
            if (!(remotefile = strtok(NULL, " \t\n")))
                print("Error: missing arguments\n");
            else {
                if(!(localfile = strtok(NULL, " \t\n")))
                    localfile = remotefile;
                do_get(remotefile, localfile);
            }
        }
        else if (!strcmp(arg, "put")) {
            if (!(localfile = strtok(NULL, " \t\n")))
                print("Error: missing arguments\n");
            else
                do_put(localfile);
        }
        else {
            printf("Unknown command!\n");
        }
    }
    
    return 0;
}
