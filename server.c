#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>
#include "server.h"
#include "pgmread.h"

int ack_nr = 0;
int expected_seq = 0;
char *dir_name;
struct dirent *entry;
DIR *directory;


const char *results_filename;
FILE *results_file;

int main(int argc, char *argv[])
{

    usage(argc, argv);

    char buf[BUFSIZ];

    int port_number = htons(atoi(argv[1]));
    dir_name = argv[2];
    printf("Comparing with: %s\n", dir_name);
    results_filename = argv[3];

    results_file = fopen(results_filename, "a+");

    int so, rc, wc;
    struct in_addr ip;
    struct sockaddr_in server_a, client_a;
    socklen_t from_len;

    server_a.sin_family = AF_INET;
    server_a.sin_port = port_number;
    server_a.sin_addr.s_addr = INADDR_ANY;

    so = socket(AF_INET, SOCK_DGRAM, 0);
    error(so, "socket");

    rc = bind(so, (struct sockaddr *)&server_a, sizeof(struct sockaddr_in));
    error(rc, "bind");

    int uniq_int = 0;

    /* While har alltid sann parameter
     * pga. enten så blir den terminert med en termineringspakke
     * eller så må den termineres manuelt. 
     */
    while (1)
    {
        from_len = sizeof(struct sockaddr_in);
        rc = recvfrom(so, buf, BUFSIZ, 0, (struct sockaddr *)&client_a, &from_len);
        error(rc, "recvfrom");
        buf[rc] = '\0';

        int packet_length = buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
        unsigned char seq_number = buf[4];
        unsigned char ack_number = buf[5];
        unsigned char flags = buf[6];
        unsigned char unused = buf[7];
        printf("Received SEQ: %u\n", seq_number);

        if (flags != CLOSES_CONNECTION && seq_number == expected_seq)
        {
            if (DEBUG_ENABLED)
                printf("SEND_ACK\n");

            payload_t *payload = create_payload(buf, rc);
            open_directory(payload->image, payload->filename);

            //send ack
            packet_t *ack_packet = create_packet();
            wc = sendto(so, ack_packet, ack_packet->packet_length, 0, (struct sockaddr *)&client_a, sizeof(struct sockaddr_in));
            error(wc, "sendto");
            printf("SENT_ACK: %d\n", ack_packet->ack_number);

            expected_seq++;

            free(payload->filename);
            Image_free(payload->image);
            free(payload);
            free(ack_packet);
        }
        else if (flags == CLOSES_CONNECTION)
        {
            printf("Got termination packet, closing connection\n");
            break;
        }
        else
        {
            printf("Expected %d, got %d\n", expected_seq, seq_number);
        }
    }
    fclose(results_file);
    exit(EXIT_SUCCESS);
}

void error(int ret, char *msg)
{
    if (-1 == ret)
    {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

payload_t *create_payload(char *buf, int rc)
{
    if (DEBUG_ENABLED)
        printf("create_payload()\n");
    payload_t *payload = malloc(sizeof(payload_t));

    payload->id = *((int *)&buf[ID_I]);
    int payload_length = *((int *)&buf[LENGTH_I]);

    payload->length = payload_length;

    payload->filename = malloc(payload_length + 1);
    char fn[payload_length + 1];
    int x = 0;
    while (x < payload_length)
    {
        fn[x] = buf[x + 16];
        x++;
    }
    fn[x] = '\0';
    memcpy(payload->filename, fn, payload_length + 1);

    int img_w = *((int *)&buf[FILENAME_I + payload_length]);
    int img_h = *((int *)&buf[FILENAME_I + payload_length + sizeof(int)]);
    int image_data_index = FILENAME_I + payload_length + sizeof(int) * 2;

    struct Image *payload_image = malloc(sizeof(struct Image));
    payload_image->height = img_h;
    payload_image->width = img_w;
    payload_image->data = malloc((img_w * img_h) + 1);

    int c = 0;
    for (int i = image_data_index; i < ((img_w * img_h) + image_data_index); i++)
    {
        payload_image->data[c] = buf[i];
        c++;
    }
    payload_image->data[c] = '\0';
    payload->image = payload_image;
    return payload;
}

packet_t *create_packet()
{
    if (DEBUG_ENABLED)
        printf("create_packet()\n");

    packet_t *packet;
    packet = malloc(sizeof(packet_t));

    packet->sequence_number = 0;
    packet->unused = UNUSED;
    packet->payload = 0;
    packet->flags = CONTAIN_ACK;
    packet->ack_number = ack_nr++;

    packet->packet_length = 8;

    return packet;
}

void usage(int argc, char *argv[])
{
    if (DEBUG_ENABLED)
        printf("usage()\n");
    if (argv[1] == NULL || argv[2] == NULL || argv[3] == NULL)
    {
        printf("ERROR! Usage:\n<server_port> <directory_name> <file_name>\n");
        exit(EXIT_FAILURE);
    }
}

void open_directory(struct Image *compare, char *payload_name)
{
    if (DEBUG_ENABLED)
        printf("open_directory()\n");
    directory = opendir(dir_name); //dir_name = "big_set/"
    if (directory == NULL)
    {
        perror("Unable to read directory");
        return;
    }

    while ((entry = readdir(directory)))
    {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
        {
            char *filename = malloc(strlen(dir_name) + strlen(entry->d_name) + 2);

            memcpy(filename, dir_name, strlen(dir_name));
            memcpy(filename + strlen(dir_name), "/", strlen("/"));
            memcpy(filename + strlen(dir_name) + strlen("/"), entry->d_name, strlen(entry->d_name));
            filename[strlen(dir_name) + strlen(entry->d_name) + 1] = '\0';
            struct Image *compare_with = read_image(filename);

            if (Image_compare(compare, compare_with))
            {
                //skriv til fil
                printf("Found match for %s\n", payload_name);
                append_to_file(payload_name, entry->d_name);
                Image_free(compare_with);
                free(filename);
                closedir(directory);
                return;
            }
            Image_free(compare_with);
            free(filename);
        }
    }
    append_to_file(payload_name, "UNKNOWN");
    closedir(directory);
}

void append_to_file(char *r_name, char *match_name)
{
    fprintf(results_file, "<%s> <%s>\n", r_name, match_name);
}

struct Image *read_image(char *filename)
{
    if (DEBUG_ENABLED)
        printf("read_image()\n");
    long line_n = 0;
    struct Image *image;

    FILE *imgfile = fopen(filename, "r");
    if (imgfile == NULL)
    {
        perror("file not found\n");
        exit(1);
    }
    // Get buf size
    fseek(imgfile, 0L, SEEK_END);
    line_n = ftell(imgfile);
    rewind(imgfile);

    //read contents of imagefile into buf
    char buf[line_n + 1];
    fread(buf, sizeof(char), line_n, imgfile);

    buf[line_n] = '\0';

    // Create image from buf
    image = Image_create(buf);
    fclose(imgfile);
    return image;
}
