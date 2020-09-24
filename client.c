#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>
#include "client.h"
#include "pgmread.h"
#include "send_packet.h"
#include "linkedlist.h"

/* -- Indeks brukt for memcpy, sette verdier i pakker -- */
#define SEQ_I (sizeof(int))
#define ACK_I (SEQ_I + sizeof(unsigned char))
#define FLAG_I (ACK_I + sizeof(unsigned char))
#define UNUSED_I (FLAG_I + sizeof(unsigned char))
#define ID_I (UNUSED_I + sizeof(unsigned char))
#define LENGTH_I (ID_I + sizeof(int))
#define FILENAME_I (LENGTH_I + sizeof(int))

/* To lenkelister:
 * all_packets_list inneholder alle pakker som skal sendes til å begynne med
 * når en pakke blir sendt, flyttes den til sent_packets_list.
 * Når ACK er mottatt for en pakke, poppes den fra sent_packets_list. 
 */
linkedlist_t *all_packets_list;
linkedlist_t *sent_packets_list;

packet_t *current_packet;

/* payload_id er unik nr i payload, som starter på 10 */
int payload_id = 10;
int sequence_nr = 0;

/* Window size for sliding window algoritmen */
int window_size = 7;

char *p;
char packet_char[BUFSIZ];

packet_t *close_packet;

/* free_node() tar imot en liste og frigjør minne
 * som brukes av nodene i listen.
 * Frigjør image, filnavn, payload, og packet i hver node
 */
void free_node(struct linkedlist *list)
{
    node_t *temp = list->start->next;
    packet_t *packet = temp->packet;
    payload_t *payload = packet->payload;
    char *fn = payload->filename;
    struct Image *image = payload->image;
    Image_free(image);
    free(fn);
    free(payload);
    free(packet);
}

int main(int argc, char *argv[])
{
    usage(argc, argv);

    char buf[BUFSIZ];

    char *ip_adresse = argv[1];
    int server_port = htons(atoi(argv[2]));
    char *filename = argv[3];
    float drop_percentage = strtof(argv[4], NULL) / 100;

    int so, wc, rc;
    node_t *send_node;
    packet_t *packet;
    struct in_addr ipadresse;
    struct sockaddr_in adresse;

    fd_set set;
    struct timeval tv;

    /* start_time for å holde styr på
     * hvor lang tid det går fra en pakke er sendt 
     * til tilhørende ACK er mottatt.
     */
    time_t start_time;
    int lower_bound = 0;
    int upper_bound = 0;

    /* Sett opp linkedlist:
    * malloc all_packets_list som inneholder alle pakker
    * malloc sent_packets_list som skal inneholde pakker i window
    * sett start og slutt node
    */
    initialize();

    printf("Server address: %s\n", ip_adresse);
    printf("Server port: %d\n", server_port);
    printf("Filename: %s\n", filename);
    printf("Drop percentage: %f\n", drop_percentage);
    set_loss_probability(drop_percentage);

    /* read_file(filename) leser listen med filnavn fra arg,
     * og tilhørende bildefiler med read_image(). Deretter
     * blir det laget payload i create_payload() og packet 
     * i create_packet() som sendes til server.
     */
    read_file(filename);

    /* Setter opp iht Cbra */
    inet_pton(AF_INET, ip_adresse, &ipadresse);
    adresse.sin_family = AF_INET;
    adresse.sin_port = server_port;
    adresse.sin_addr = ipadresse;

    so = socket(AF_INET, SOCK_DGRAM, 0);
    error(so, "socket");

    // send_node er noden der pakken skal sendes. Blir neste til startnoden.
    // packet er pakken i noden.
    send_node = all_packets_list->start->next;
    packet = send_node->packet;

    /* Bruker 2 time_t variabler for å holde styr på tid.
     * Når en pakke blir sendt, blir tiden satt i sent_time
     * og lagt til i tilhørende node. 
     * Deretter når det skal sjekke om ACK kommer innen 5 sekunder,
     * blir nåværende tid i current_time sammenlikned med sent_time. 
     */
    time_t sent_time;
    time_t current_time;

    // while (lower_bound < sequence_nr - 2)
    while (all_packets_list->size > 0)
    {
        while (all_packets_list->size > 0 && (upper_bound - lower_bound) < window_size)
        {
            if (DEBUG_ENABLED)
                printf("SENDING\n");

            // Gjør om packet til char som kan sendes, legger inn i char packet_char
            packet_to_char(packet);
            p = packet_char;

            /* Sett nåværende tid i sent_time. 
             * lagres i variabelen i noden som ble sendt
             * ved push
             */
            time(&sent_time);
            wc = send_packet(so, p, packet->packet_length, 0, (struct sockaddr *)&adresse, sizeof(struct sockaddr_in));
            error(wc, "sendto");
            printf("SENT %u, (%d)\n", packet->sequence_number, wc);
            upper_bound++;

            send_node = send_node->next;
            push(sent_packets_list, packet, sent_time);
            pop(all_packets_list);
            if (send_node != all_packets_list->end)
            {
                packet = send_node->packet;
            }
        }

        //Motta ACK
        FD_ZERO(&set);
        int i = 0;
        time(&current_time);

        /* Når ACK skal mottas, sjekkes det at tiden fra pakken ble sent og nåværende tid er under 5 sekunder
         * samt vi er på window size ELLER at alle pakker har blitt sendt. Hvis ikke, gå til neste loop
         */
        while (difftime(current_time, sent_packets_list->start->next->time_sent) < 5 && ((upper_bound - lower_bound) == window_size || all_packets_list->size == 0))
        {
            packet_t *to_receive = sent_packets_list->start->next->packet;
            printf("EXPECTING: %d\n", to_receive->sequence_number);
            tv.tv_sec = 0;
            tv.tv_usec = 500000;
            FD_SET(so, &set);

            rc = select(FD_SETSIZE, &set, NULL, NULL, &tv);
            error(rc, "select");

            if (FD_ISSET(so, &set))
            {
                rc = read(so, buf, BUFSIZ - 1);
                error(rc, "read");
                printf("Received ACK: %d for SEQ: %d\n", buf[5], to_receive->sequence_number);
                free_node(sent_packets_list);
                pop(sent_packets_list);
                buf[rc - 1] = '\0';
                lower_bound++;
            }
            if (sent_packets_list->start->next == sent_packets_list->end)
                break; //Bryt løkken vi har nådd slutten av listen, alle ACK er mottatt. 
            time(&current_time);
        }

        //send utløpte pakker på nytt
        // if (sent_packets_list->size > 0 && current_packet->flags != CLOSES_CONNECTION)
        time(&current_time);
        while (sent_packets_list->size > 0 && difftime(current_time, sent_packets_list->start->next->time_sent) >= 5)
        {
            if (DEBUG_ENABLED)
                printf("RESEND\n");

            node_t *current_node = sent_packets_list->start->next;
            current_packet = current_node->packet;
            upper_bound -= sent_packets_list->size;

            while (current_node != sent_packets_list->end)
            {
                //sender pakker på nytt
                time(&current_node->time_sent);
                packet_to_char(current_packet);
                p = packet_char;
                wc = send_packet(so, p, current_packet->packet_length, 0, (struct sockaddr *)&adresse, sizeof(struct sockaddr_in));
                error(wc, "sendto");
                printf("SENT %d (%f)\n", current_packet->sequence_number, difftime(current_time, current_node->time_sent));
                current_node = current_node->next;
                current_packet = current_node->packet;
                upper_bound++;
            }
        }
    }

    //close connection
    packet_to_char(close_packet);
    wc = send_packet(so, packet_char, close_packet->packet_length, 0, (struct sockaddr *)&adresse, sizeof(struct sockaddr_in));
    error(wc, "sendto");
    printf("SENT_TERMINATION\n");
    // Frigjor minne
    free_all();
    close(so);
    printf("FERDIIIIG\n");
    return 0;
}

void packet_to_char(packet_t *packet)
{
    if (DEBUG_ENABLED)
        printf("packet_to_char()\n");

    memcpy(packet_char, &packet->packet_length, sizeof(int));
    if (DEBUG_ENABLED)
        printf("PACKET_LENGTH: %d\n", packet->packet_length);

    memcpy(packet_char + SEQ_I, &packet->sequence_number, sizeof(char));
    if (DEBUG_ENABLED)
        printf("SEQ_NR: %d\n", packet->sequence_number);

    memcpy(packet_char + ACK_I, &packet->ack_number, sizeof(char));
    if (DEBUG_ENABLED)
        printf("ACK_NR: %d\n", packet->ack_number);

    memcpy(packet_char + FLAG_I, &packet->flags, sizeof(char));
    if (DEBUG_ENABLED)
        printf("FLAGS: %d\n", packet->flags);

    memcpy(packet_char + UNUSED_I, &packet->unused, sizeof(char));
    if (DEBUG_ENABLED)
        printf("UNUSED: %d\n", packet->unused);

    if (packet->flags != CLOSES_CONNECTION)
    {
        memcpy(packet_char + ID_I, &packet->payload->id, sizeof(int));
        if (DEBUG_ENABLED)
            printf("UNIQUE_NR: %d\n", packet->payload->id);

        memcpy(packet_char + LENGTH_I, &packet->payload->length, sizeof(int));
        if (DEBUG_ENABLED)
            printf("PAYLOAD_LENGTH: %d\n", packet->payload->length);

        int name_length = packet->payload->length;
        memcpy(packet_char + FILENAME_I, packet->payload->filename, name_length);
        if (DEBUG_ENABLED)
            printf("PAYLOAD_NAME: %s\n", packet->payload->filename);

        long img_w = FILENAME_I + name_length;
        int img_w_data = packet->payload->image->width;
        memcpy(packet_char + img_w, &img_w_data, sizeof(int));
        if (DEBUG_ENABLED)
            printf("IMG_WIDTH: %d\n", packet->payload->image->width);

        long img_h = img_w + sizeof(int);
        int img_h_data = packet->payload->image->height;
        memcpy(packet_char + img_h, &img_h_data, sizeof(int));
        if (DEBUG_ENABLED)
            printf("IMG_HEIGHT: %d\n", packet->payload->image->height);

        long img_d = img_h + sizeof(int);
        int img_data_data = img_w_data * img_h_data;
        memcpy(packet_char + img_d, packet->payload->image->data, img_data_data);
    }
}

void free_all()
{
    if (DEBUG_ENABLED)
        printf("free_all()\n");
    /* CLEANUP */

    //all_packets
    node_t *first = all_packets_list->start->next;
    while (first != all_packets_list->end)
    {
        packet_t *packet = first->packet;
        payload_t *payload = packet->payload;
        if (payload)
        {
            free(payload->filename);
            struct Image *img = payload->image;
            Image_free(img);
        }
        free(payload);
        free(packet);
        node_t *temp_node = first;
        first = first->next;
        free(temp_node);
    }

    //sent_packets
    node_t *first_sent = sent_packets_list->start->next;
    while (first_sent != sent_packets_list->end)
    {
        packet_t *packet = first_sent->packet;
        payload_t *payload = packet->payload;
        if (payload)
        {
            free(payload->filename);
            struct Image *img = payload->image;
            Image_free(img);
        }
        free(payload);
        free(packet);
        node_t *temp_node = first_sent;
        first_sent = first_sent->next;
        free(temp_node);
    }

    free(close_packet);
    free(all_packets_list->start);
    free(all_packets_list->end);
    free(sent_packets_list->start);
    free(sent_packets_list->end);
    free(all_packets_list);
    free(sent_packets_list);
}

void initialize()
{
    if (DEBUG_ENABLED)
        printf("initialize()\n");
    all_packets_list = malloc(sizeof(linkedlist_t));
    sent_packets_list = malloc(sizeof(linkedlist_t));

    all_packets_list->start = malloc(sizeof(node_t));
    all_packets_list->end = malloc(sizeof(node_t));
    all_packets_list->size = 0;

    sent_packets_list->start = malloc(sizeof(node_t));
    sent_packets_list->end = malloc(sizeof(node_t));
    sent_packets_list->size = 0;

    all_packets_list->start->next = all_packets_list->end;
    all_packets_list->end->prev = all_packets_list->start;

    sent_packets_list->start->next = sent_packets_list->end;
    sent_packets_list->end->prev = sent_packets_list->start;
}

void usage(int argc, char *argv[])
{
    if (DEBUG_ENABLED)
        printf("usage()\n");
    if (argc != 5)
    {
        printf("%d\n", argc);
        printf("Usage:\n./client <server_ip> <server_port> <src_filename> <drop_percentage>\n");
        exit(EXIT_FAILURE);
    }
    if (atoi(argv[4]) > 20 || atoi(argv[4]) < 0)
    {
        printf("Drop percentage should be between 0 and 20, you entered %d\n", atoi(argv[4]));
        exit(EXIT_FAILURE);
    }
}

void error(int ret, char *msg)
{
    if (ret == -1)
    {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void read_file(char *files_list)
{
    if (DEBUG_ENABLED)
        printf("read_file()\n");
    FILE *entry_file;
    int counter;
    counter = 0;
    char buf[BUFSIZ];
    entry_file = fopen(files_list, "r");
    while (fgets(buf, BUFSIZ, entry_file))
    {
        read_image(strtok(buf, "\n"));
        counter++;
    }
    create_packet(NULL, CLOSES_CONNECTION);
    fclose(entry_file);
}

void read_image(char *filename)
{
    // printf("%s\n", filename);
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
    create_payload(image, filename);
    fclose(imgfile);
}

void create_payload(struct Image *image, char *name)
{
    if (DEBUG_ENABLED)
        printf("create_payload()\n");

    payload_t *payload = malloc(sizeof(payload_t));

    strtok(name, "/");
    char *filename = strtok(NULL, "\n");
    int length_name = strlen(filename) + 1;

    payload->id = payload_id++;
    payload->length = length_name;

    payload->filename = malloc(length_name);
    if (NULL == payload->filename)
        error(-1, "payload_filename\n");

    memcpy(payload->filename, filename, length_name);

    payload->image = image;

    create_packet(payload, CONTAIN_DATA);
}

void create_packet(payload_t *payload, unsigned char flag)
{
    if (DEBUG_ENABLED)
        printf("create_packet()\n");

    packet_t *packet;
    packet = malloc(sizeof(packet_t));

    packet->sequence_number = sequence_nr++;
    packet->unused = UNUSED;
    packet->payload = payload;
    packet->flags = flag;
    packet->ack_number = 0;

    packet->packet_length = get_packet_length(payload);

    if (flag != CLOSES_CONNECTION)
    {
        push(all_packets_list, packet, 0);
    }
    else
    {
        close_packet = packet;
    }
}

int get_packet_length(payload_t *payload)
{
    if (DEBUG_ENABLED)
        printf("get_packet_length()\n");
    int len = 0;

    if (!payload)
    {
        return sizeof(int) + (sizeof(unsigned char) * 4);
    }

    //image
    len += payload->image->height * payload->image->width; //data (width*height)
    len += sizeof(int) * 2;                                //int width, height

    //payload
    len += sizeof(int) * 2; //int id, length
    len += payload->length; //filename length

    //packet
    len += sizeof(int);      //packet_length
    len += sizeof(char) * 4; //char sequence_number, ack_number, flags, unused

    return len;
}