#include "server.h"

unsigned short server_port = SERVER_PORT;

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

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        err_sys("socket");

    if (bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        err_sys("bind");

    if (peer) {
        if (connect(sock, (struct sockaddr *)peer, sizeof(struct sockaddr_in)) < 0)
            err_sys("connect");
    }

    return sock;
}

static void handle_rrq(int sockfd, struct tftpreq *request)
{

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

    if (close(connectfd) < 0)
        err_sys("close connectfd");
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

    while (true) {
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
