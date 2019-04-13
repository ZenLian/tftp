#include "server.h"

unsigned short server_port = SERVER_PORT;
char root[128] = "/tftpboot";

static inline void err_usage(char **argv)
{
    printf("Usage: %s [-p port]\n", argv[0]);
    exit(EXIT_FAILURE);
}

static int handle_arg(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
        case 'p':
            server_port = atoi(optarg);
            break;
        default:
            err_usage(argv);
        }
    }

    return 0;
}

static int create_socket(unsigned short port, struct sockaddr_in *peer)
{
    int sock;
    struct sockaddr_in serveraddr;

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port   = htons(port);
    serveraddr.sin_addr.s_addr = htons(INADDR_ANY);// TODO: sin_addr or s_addr? htons or not?

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("bind");
        return -1;
    }

    if (peer)
        if (connect(sock, (struct sockaddr *)peer, sizeof(struct sockaddr_in)) < 0) {
            perror("connect");
            return -1;
        }

    return sock;
}

static void handle_rrq(int sockfd, struct tftpreq *request)
{
    struct tftphdr *req_hdr = (struct tftphdr *)(request->packet);
    char fullpath[256];
    char *rpath = req_hdr->th_stuff;
    char *mode  = rpath + strlen(rpath) + 1;
    char snd_pkt[PACKET_BUF_SIZE], rcv_pkt[PACKET_BUF_SIZE];
    struct tftphdr* snd_hdr = (struct tftphdr *)snd_pkt;
    struct tftphdr* rcv_hdr = (struct tftphdr *)rcv_pkt;
    struct timeval timeout;
    FILE *fp;
    int read_len, snd_len, rcv_len;
    unsigned short block = 1;
    int last = 0, resend = 0;
    int ret;
    fd_set selectfd;

    if (strncasecmp(mode, "netascii", 8) && strncasecmp(mode, "octet", 5)) {
        printf("mode not supported!\n");
        //TODO: send error
        return;
    }

    // build fullpath
    //build_path(rpath, fullpath, 256);
    strcpy(fullpath, root);
    if (rpath[0] != '/')
        strcat(fullpath, "/");
    strcat(fullpath, rpath);

    // open file for read
    if ((fp = fopen("fullpath", "r")) == NULL) {
        printf("file not found!\n");
        //TODO: send ENOTFOUND
        return;
    }

    // send DATA and wait for ACK
    while (1) {
        if (!resend) {
            memset(snd_pkt, 0, PACKET_BUF_SIZE);

            // read from local file
            if ((read_len = fread(snd_hdr->th_data, 1, BLOCK_SIZE, fp)) != BLOCK_SIZE) {
                if (feof(fp)) {
                    last = 1;
                }
                else {
                    //TODO: send error
                    goto rrq_end;
                }
            }

            snd_hdr->th_opcode = htons(DATA);
            snd_hdr->th_block  = htons(block);
            snd_len = read_len + 4;
        }

        // send DATA
        if (send(sockfd, snd_pkt, snd_len, 0) < 0) {
            printf("error: send DATA: block %u, size %u\n", block, read_len);
            goto rrq_end;
        }

        // wait for ACK
        FD_ZERO(&selectfd);
        FD_SET(sockfd, &selectfd);
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;
        ret = select(sockfd+1, &selectfd, NULL, NULL, &timeout);
        if (ret > 0) {
            // recv ACK
            memset(rcv_pkt, 0, PACKET_BUF_SIZE);
            if ((rcv_len = recv(sockfd, rcv_pkt, PACKET_BUF_SIZE, 0)) < 0) {
                printf("error: recv ACK: block %u\n", block);
                goto rrq_end;
            }

            if (rcv_len == 4 && rcv_hdr->th_opcode == htons(ACK) && rcv_hdr->th_block == htons(block)) {
                if (last) {
                    break;
                }
                else {
                    ++block;
                    resend = 0;
                    continue;
                }
            }
        } else if (ret == 0) {
            // TIMEOUT: resend previous DATA
            if (++resend > 5) {
                printf("fail: select: wait for ACK timeout \n");
                goto rrq_end;
            }
        } else {
            printf("error: select\n");
            goto rrq_end;
        }
    }

rrq_end:
    if (fclose(fp) != 0) {
        printf("error: fclose\n");
        return;
    }
}

static void handle_wrq(int sockfd, struct tftpreq *request)
{

}

// TODO: work thread cannot call exit(3) when encountering mistake
static void * work_thread(void *arg)
{
    struct tftpreq *request = (struct tftpreq *)arg;
    struct tftphdr *req_hdr = (struct tftphdr *)(request->packet);
    int connectfd;

    connectfd = create_socket(0, &(request->client));
    if (connectfd == -1) {
        printf("error: cannot create socket\n");
        goto work_thread_end;
    }

    switch (htons(req_hdr->th_opcode)) {
    case RRQ:
        handle_rrq(connectfd, request);
        break;
    case WRQ:
        handle_wrq(connectfd, request);
        break;
    default:
        // TODO: send ERROR
        break;
    }

    if (close(connectfd) < 0) {
        perror("close connectfd");
        goto work_thread_end;
    }

work_thread_end:
    free(request);
    return NULL;
}

int main(int argc, char **argv)
{
    int listenfd;
    struct tftpreq *request;
    socklen_t socklen;
    pthread_t tid;

    handle_arg(argc, argv);

    listenfd = create_socket(server_port, NULL);

    while (1) {
        if ((request = (struct tftpreq *)malloc(sizeof(struct tftpreq))) == NULL)
            err_sys("malloc for tftpreq");
        socklen = sizeof(struct sockaddr_in);
        if ((request->len = recvfrom(listenfd, &(request->packet), PACKET_BUF_SIZE, 0, (struct sockaddr *)&(request->client), &socklen)) <= 0)
            err_sys("recvfrom request");
        if (pthread_create(&tid, NULL, work_thread, request))
            err_sys("pthread_create");
    }

    return 0;
}
