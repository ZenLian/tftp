#ifndef _TFTP_H
#define _TFTP_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>

/*
 * Packet types.
 */
#define RRQ   01                /* read request */
#define WRQ   02                /* write request */
#define DATA  03                /* data packet */
#define ACK   04                /* acknowledgement */
#define ERROR 05                /* error code */

struct tftphdr {
    short    th_opcode;                            /* packet type */
    union {
        char tu_padding[3];                        /* sizeof() compat */
        struct {
            union {
                unsigned short tu_block;           /* block # */
                short tu_code;                     /* error code */
            } __attribute__ ((__packed__)) th_u3;
            char tu_data[0];                       /* data or error string */
        } __attribute__ ((__packed__)) th_u2;
        char tu_stuff[0];                          /* request packet stuff */
    } __attribute__ ((__packed__)) th_u1;
} __attribute__ ((__packed__));

#define th_block    th_u1.th_u2.th_u3.tu_block
#define th_code     th_u1.th_u2.th_u3.tu_code
#define th_stuff    th_u1.tu_stuff
#define th_data     th_u1.th_u2.tu_data
#define th_msg      th_u1.th_u2.tu_data
/*
 * Error codes.
 */
#define EUNDEF       0        /* not defined */
#define ENOTFOUND    1        /* file not found */
#define EACCESS      2        /* access violation */
#define ENOSPACE     3        /* disk full or allocation exceeded */
#define EBADOP       4        /* illegal TFTP operation */
#define EBADID       5        /* unknown transfer ID */
#define EEXISTS      6        /* file already exists */
#define ENOUSER      7        /* no such user */


#define SERVER_PORT       6969
#define CMDLINE_BUF_SIZE  1024
#define BLOCK_SIZE        512
#define PACKET_BUF_SIZE   516

static inline void err_sys(const char *str)
{
    perror(str);
    exit(-1);
}

static inline void err_quit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    exit(-1);
}

#endif /* _TFTP_H */
