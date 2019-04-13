#include "tftp.h"

int sock;
struct sockaddr_in server;
char mode[] = "netascii";

static inline void err_usage(char **argv)
{
    printf("Usage: %s host [port]\n", argv[0]);
    exit(-1);
}

static int handle_arg(int argc, char **argv)
{
    char *server_ip;
    unsigned short server_port = SERVER_PORT;

    if (argc < 2 || argc > 3)
        return -1;

    server_ip = argv[1];

    if (argc > 2)
        server_port = atoi(argv[2]);

    server.sin_family = AF_INET;
    server.sin_port   = htons(server_port);
    if (inet_pton(AF_INET, argv[1], &(server.sin_addr.s_addr)) <= 0)
        err_quit("invalid AF or address");

    printf("Usage:\n");
    printf("> get remotefile [localfile]\n");
    printf("> put localfile  [remotefile]\n");

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        err_sys("socket");
    printf("connect to %s:%u\n", server_ip, server_port);

    return 0;
}

void do_get(char *remotefile, char *localfile)
{
    int ret;
    char snd_pkt[PACKET_BUF_SIZE], rcv_pkt[PACKET_BUF_SIZE];
    struct tftphdr* snd_hdr = (struct tftphdr *)snd_pkt;
    struct tftphdr* rcv_hdr = (struct tftphdr *)rcv_pkt;
    struct timeval timeout;
    struct sockaddr_in connect_addr;
    socklen_t socklen;
    int len;
    unsigned short block = 1;
    FILE *fp;
    fd_set selectfd;

    /* open localfile for write */
    if ((fp = fopen(localfile, "w")) == NULL)
        err_sys("fopen");

    /* send RRQ */
    snd_hdr->th_opcode = htons(RRQ);
    len = sprintf(snd_hdr->th_stuff, "%s%c%s%c", remotefile, 0, mode, 0);
    len += 2; /* include opcode len */
    socklen = sizeof(server);
    if ((ret = sendto(sock, snd_pkt, len, 0, (struct sockaddr *)&server, socklen)) != len) {
        err_sys("sendto");
    }

    memset(snd_pkt, 0, PACKET_BUF_SIZE);
    while (1) {
        /* wait for data*/
        FD_ZERO(&selectfd);
        FD_SET(sock, &selectfd);
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;
        ret = select(sock+1, &selectfd, NULL, NULL, &timeout);
        if (ret > 0) {
            /* recv DATA */
            memset(rcv_pkt, 0, PACKET_BUF_SIZE);
            socklen = sizeof(connect_addr);
            if ((len = recvfrom(sock, rcv_pkt, PACKET_BUF_SIZE, 0, (struct sockaddr *)&connect_addr, &socklen)) < 0)
                err_sys("recvfrom");

            if (len >= 4 && rcv_hdr->th_opcode == htons(DATA) && rcv_hdr->th_block == htons(block)) {
                /* send ACK */
                snd_hdr->th_opcode = htons(ACK);
                snd_hdr->th_block  = rcv_hdr->th_block;
                if (sendto(sock, snd_pkt, 4, 0, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) != 4)
                    err_sys("sendto ack");
                block++;
                /* write to localfile */
                len -= 4; /* exclude opcode and block */
                if (fwrite(rcv_hdr->th_data, 1, len, fp) != len)
                    err_sys("fwrite");
                if (len < BLOCK_SIZE) {
                    break;
                }
            }
            else
                continue;

        }
        else if (ret == 0) {
            err_quit("timeout");
        }
        else
            err_sys("select");
    }

    if (fclose(fp) != 0)
        err_sys("fclose");
    return;
}

void do_put(char *localfile, char *remotefile)
{
    int ret;
    char snd_pkt[PACKET_BUF_SIZE], rcv_pkt[PACKET_BUF_SIZE];
    struct tftphdr* snd_hdr = (struct tftphdr *)snd_pkt;
    struct tftphdr* rcv_hdr = (struct tftphdr *)rcv_pkt;
    struct timeval timeout;
    struct sockaddr_in connect_addr;
    socklen_t socklen;
    int len;
    unsigned short block = 0;
    FILE *fp;
    fd_set selectfd;
    int last = 0;

    /* open local file for read */
    if ((fp = fopen(localfile, "r")) == NULL)
        err_sys("fopen");

    /* send WRQ */
    snd_hdr->th_opcode = htons(WRQ);
    len = sprintf(snd_hdr->th_stuff, "%s%c%s%c", remotefile, 0, mode, 0);
    len += 2; /* include opcode len */
    socklen = sizeof(server);
    if ((ret = sendto(sock, snd_pkt, len, 0, (struct sockaddr *)&server, socklen)) != len) {
        err_sys("sendto");
    }

    memset(snd_pkt, 0, PACKET_BUF_SIZE);
    while (1) {
        /* wait for ACK */
        FD_ZERO(&selectfd);
        FD_SET(sock, &selectfd);
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;
        ret = select(sock+1, &selectfd, NULL, NULL, &timeout);
        if (ret > 0) {
            /* recv ACK */
            memset(rcv_pkt, 0, PACKET_BUF_SIZE);
            socklen = sizeof(connect_addr);
            if ((len = recvfrom(sock, rcv_pkt, PACKET_BUF_SIZE, 0, (struct sockaddr *)&connect_addr, &socklen)) < 0)
                err_sys("recvfrom");

            if (len == 4 && rcv_hdr->th_opcode == htons(ACK) && rcv_hdr->th_block == htons(block)) {
                if (last)
                    break;
                /* read from localfile */
                if ((len = fread(snd_hdr->th_data, 1, BLOCK_SIZE, fp)) != BLOCK_SIZE) {
                    if (feof(fp))
                        last = 1;
                    else //if(ferror(fp))
                        err_sys("fread");
                }
                /* send DATA */
                snd_hdr->th_opcode = htons(DATA);
                snd_hdr->th_block  = htons(++block);
                len += 4; /* include opcode and block */
                if (sendto(sock, snd_pkt, len, 0, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) != len)
                    err_sys("sendto ack");
            }
            else
                continue;
        }
        else if (ret == 0) {
            err_quit("timeout");
        }
        else
            err_sys("select");
    }

    if (fclose(fp) != 0)
        err_sys("fclose");
    return;
}

int main(int argc, char **argv)
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
            printf("Bye!\n");
            return 0;
        }

        arg = strtok(cmdline, " \t\n");
        if (arg == NULL)
            continue;

        if (!strcmp(arg, "get")) {
            if (!(remotefile = strtok(NULL, " \t\n")))
                printf("Error: missing arguments\n");
            else {
                if (!(localfile = strtok(NULL, " \t\n")))
                    localfile = remotefile;
                do_get(remotefile, localfile);
            }
        }
        else if (!strcmp(arg, "put")) {
            if (!(localfile = strtok(NULL, " \t\n")))
                printf("Error: missing arguments\n");
            else {
                if (!(remotefile = strtok(NULL, " \t\n")))
                    remotefile = localfile;
                do_put(localfile, remotefile);
            }
        }
        else {
            printf("Error: Unknown command!\n");
        }
    }

    return 0;
}
