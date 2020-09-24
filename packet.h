#ifndef PACKET_H
#define PACKET_H

#define CONTAIN_DATA 0x1
#define CONTAIN_ACK 0x2
#define CLOSES_CONNECTION 0x4
#define UNUSED 0x7f
#define DEBUG 0

typedef struct payload {
    int id;
    int length;
    char *filename;
    struct Image *image;
} payload_t;

typedef struct packet {
    int packet_length;
    unsigned char sequence_number;
    unsigned char ack_number;
    unsigned char flags;
    unsigned char unused;
    struct payload *payload;
} packet_t;


#endif
