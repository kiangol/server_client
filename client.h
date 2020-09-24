#ifndef CLIENT_H
#define CLIENT_H

#define CONTAIN_DATA 0x1
#define CONTAIN_ACK 0x2
#define CLOSES_CONNECTION 0x4
#define UNUSED 0x7f
#define DEBUG_ENABLED 0 //sett til 1 for å få info når funksjoner kjører

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

void usage(int argc, char *argv[]);
void error(int ret, char *msg);
void initialize();
void free_all();
void read_file(char *files_list);
void read_image(char *filename);
void create_payload(struct Image *image, char *name);
void create_packet(struct payload *p, unsigned char flag);
int get_packet_length(struct payload *pl);
unsigned char make_flag();
void packet_to_char(struct packet *packet);

#endif
