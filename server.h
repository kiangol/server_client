#ifndef SERVER_H
#define SERVER_H

#define CONTAIN_DATA 0x1
#define CONTAIN_ACK 0x2
#define CLOSES_CONNECTION 0x4
#define UNUSED 0x7f
#define DEBUG_ENABLED 0

#define SEQ_I (sizeof(int))
#define ACK_I (SEQ_I + sizeof(unsigned char))
#define FLAG_I (ACK_I + sizeof(unsigned char))
#define UNUSED_I (FLAG_I + sizeof(unsigned char))
#define ID_I (UNUSED_I + sizeof(unsigned char))
#define LENGTH_I (ID_I + sizeof(int))
#define FILENAME_I (LENGTH_I + sizeof(int))

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

void initialize();
void error(int ret, char *msg);
void usage(int argc, char *argv[]);
payload_t* create_payload(char *buf, int rc);
packet_t *create_packet();
void free_mem();
void open_directory(struct Image *compare, char *payload_name);
void append_to_file(char *r_name, char *match_name);
struct Image *read_image(char *filename);


#endif
