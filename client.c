/* client.c
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dlfcn.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#include "library.h"
#include "audio.h"
#include "packet.h"

// Minimal amount of (micro)seconds to wait for a server reply
#define SERVER_TIMEOUT_SEC 1
#define SERVER_TIMEOUT_USEC 0

// Number of timeout iterations before assuming server is down or busy
#define SERVER_TIMEOUT_THRESHOLD 5

// Global desciptors for the sigint_handler
int server_fd_g;
int audio_fd_g;

// Determine the frequency of printing feedback during streaming. This allows
// you to prevent your stdout from flooding. Minimal value is 1.
int print_frequency = 1;

static int breakloop = 0;

/** Attempt graceful close when Ctrl^C is pressed
 *
 * @param sigint the type of signal received
 */
void sigint_handler(int sigint)
{
	if (!breakloop){
		breakloop=1;
		printf("\tSIGINT catched. Please wait to let the client close gracefully.\nTo close hard press Ctrl^C again.\n");

        if(server_fd_g)
            close(server_fd_g);
        if(audio_fd_g)
            close(audio_fd_g);

        exit(0);
    }
	else{
       	printf ("SIGINT occurred, exiting hard... please wait\n");
		exit(-1);
	}
}

/** Receive a packet from the server
 *
 * @param read_set      a pointer to the set containing all descriptiors
 * @param server_fd     the server descriptor
 * @param packet        a pointer to the packet which will store the received packet
 * @param sec           the amount of seconds to wait for a timeout
 * @param usec          the amount of microseconds to wait for a timeout
 * @return  Success: the number of bytes read
 *          Failure: 0 if a timeout occurred, <0 otherwise
 */
int receive_packet(fd_set *read_set, int server_fd, struct audio_packet *packet, long sec, long usec) {
    int bytesread, nb;
    struct timeval timeout;
    char packet_buffer[sizeof(struct audio_packet)];

    timeout.tv_sec  = sec;
    timeout.tv_usec = usec;

    // Set a timeout for the packet
    FD_ZERO(read_set);
    FD_SET(server_fd, read_set);

    nb = select(server_fd + 1, read_set, NULL, NULL, &timeout);
    if(nb < 0) {
        perror("Error waiting for timeout");
        return -1;
    } else
    if(nb == 0) {
        // Server failed to deliver a packet in time
        return 0;
    } else
    if(FD_ISSET(server_fd, read_set)) {
        bytesread = read(server_fd, packet_buffer, sizeof(struct audio_packet));
        if(bytesread < 0)
            return bytesread;

        // Cast the received buffer to a packet struct and copy the data to the original packet
        struct audio_packet *received_packet = (struct audio_packet *) packet_buffer;
        packet->seq = received_packet->seq;
        packet->audiobytesread = received_packet->audiobytesread;
        memcpy(packet->buffer, received_packet->buffer, packet->audiobytesread);

        return packet->audiobytesread;
    } else {
        printf("Failed to process timeout result\n");
        return -1;
    }
}

/** Resolve the hostname of an IP
 *
 * @param name   The hostname
 *
 * @return  The IP address string
 */
char *resolve_hostname(char *name) {
    struct hostent *resolve;
    struct in_addr *addr;

    resolve = gethostbyname(name);

    if(resolve == NULL) {
        printf("Address not found for %s\n", name);
        exit(-1);
    }

    addr = (struct in_addr*) resolve->h_addr_list[0];
    return inet_ntoa(*addr);
}

/** Setup the client socket
 *
 * @param port      the port to bind
 * @param flen      a pointer to an integer containing the size of from
 * @param server    a structure where the source address of the datagram will be copied to
 *
 * @return  The socket identifier on succes, <0 otherwise
 */
int setup_socket(char *ip, int port, struct sockaddr_in *server) {
    int server_fd;

    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(server_fd < 0) {
        perror("Error initializing socket");
        return -1;
    }

    server_fd_g = server_fd;

    server->sin_family = AF_INET;
    server->sin_port = htons(port);
    server->sin_addr.s_addr = inet_addr(ip);

    return server_fd;
}

/** Open the audio output
 * @param info  the audio info packet
 *
 * @return the audio descriptor on success, <0 otherwise
 */
int open_output(struct audio_info *info) {
    int audio_fd;

    audio_fd = aud_writeinit(info->sample_rate, info->sample_size, info->channels);
    if (audio_fd < 0){
        printf("error: unable to open audio output.\n");
        return -1;
    }

    audio_fd_g = audio_fd;

    return audio_fd;
}

/** Receive the initial audio info packet
 * @param server_fd     the server descriptor
 * @param info_buffer   the buffer to load the bytes from the audio packet into
 *
 * @return the number of bytes read on success, <0 otherwise
 */
int receive_audio_info(int server_fd, char *info_buffer) {
    int bytesread, nb;
    struct timeval timeout;
    fd_set read_set;

    // Set a timeout for the initial packet
    FD_ZERO(&read_set);
    FD_SET(server_fd, &read_set);
    timeout.tv_sec = SERVER_TIMEOUT_SEC;
    timeout.tv_usec = SERVER_TIMEOUT_USEC;

    nb = select(server_fd + 1, &read_set, NULL, NULL, &timeout);

    // Parse the result of the timeout
    if(nb < 0) {
        perror("Error waiting for timeout");
        return -1;
    } else
    if(nb == 0) {
        printf("No reply, assuming that server is busy or down\n");
        return -1;
    } else
    if(FD_ISSET(server_fd, &read_set)) {
        bytesread = read(server_fd, info_buffer, sizeof(struct audio_info));
    }

    return bytesread;
}

/** Receive data packets from the stream and write them to audio
 * @param server_fd         the server descriptor
 * @param audio_fd          the audio descriptor
 * @param time_per_packet   the time it takes to write a packet to audio
 *
 * @return 0 on success, <0 otherwise
 */
int parse_stream(int server_fd, int audio_fd, long time_per_packet) {
    int audiobytesread, packet_timeouts, seq_expected, i;
    long packet_timeout_sec, packet_timeout_usec;
    fd_set read_set;
    struct audio_packet packet;

    packet_timeout_sec  = SERVER_TIMEOUT_SEC;
    packet_timeout_usec = SERVER_TIMEOUT_USEC + (time_per_packet / 1000);
    packet_timeouts = 0;
    seq_expected = 0;

    audiobytesread = receive_packet(&read_set, server_fd, &packet, packet_timeout_sec, packet_timeout_usec);

    while (audiobytesread == BUFSIZE || audiobytesread == 0) {
        if(audiobytesread == 0) {
            if(packet_timeouts == SERVER_TIMEOUT_THRESHOLD) {
                printf("Too many timeouts, aborting\n");
                return -1;
            }
            printf("Waiting for reply (%d)...\n", packet_timeouts);
            packet_timeouts ++;
        } else {
            if(i ++ % print_frequency == 0)
                printf("Read %d (%d) audio bytes (packet sequence number: %d)\n", packet.audiobytesread, audiobytesread, packet.seq);

            if(packet.seq == seq_expected) {
                write(audio_fd, packet.buffer, audiobytesread);
            } else {
                printf("Wrong packet, expected %d but received %d\n", seq_expected, packet.seq);
            }

            seq_expected ++;
        }

        audiobytesread = receive_packet(&read_set, server_fd, &packet, packet_timeout_sec, packet_timeout_usec);
    }

    if(audiobytesread > 0) {
        printf("Read %d (%d) audio bytes (packet sequence number: %d)\n", packet.audiobytesread, audiobytesread, packet.seq);

        write(audio_fd, packet.buffer, packet.audiobytesread);
    }

    return 0;
}

/** Process the server response from the request
 *
 * @param status    the status of the server response
 * @return 0 on success, <0 otherwise
 */
int process_request_response(enum flag status) {
    printf("Server response: ");

    switch(status) {
        case SUCCESS:
            printf("SUCCESS\n");
            return 0;
        case FILE_NOT_FOUND:
            printf("FILE_NOT_FOUND\n");
            break;
        case LIBRARY_NOT_FOUND:
            printf("LIBRARY_NOT_FOUND\n");
            break;
        case LIBRARY_ARG_NOT_ALLOWED:
            printf("LIBRARY_ARG_NOT_ALLOWED\n");
            break;
        case LIBRARY_ARG_REQUIRED:
            printf("LIBRARY_ARG_REQUIRED\n");
            break;
        case FAILURE:
            printf("FAILURE\n");
            break;
        default:
            printf("UNEXPECTED ERROR\n");
    }

    return -1;
}

/** Request the server to initialize a streaming session for a file
 * @param server_fd the server descriptor
 * @param server    the server socket identifier
 * @param filename  the filename to request
 * @param info      the audio info packet to store the data in
 *
 * @return 0 on success, <0 otherwise
 */
int send_request(int server_fd, struct sockaddr_in *server, struct request_packet *request, struct audio_info *info) {
    int err;
    char info_buffer[sizeof(struct audio_info)];
    struct audio_info *response;

    // Send the request
    err = sendto(server_fd, request, sizeof(struct request_packet), 0, (struct sockaddr*) server, sizeof(struct sockaddr_in));
    if(err < 0) {
        perror("Error sending packet");
        return -1;
    }

    // Receive the information about the audio file
    err = receive_audio_info(server_fd, info_buffer);
    if(err < 0) {
        return err;
    }

    response = (struct audio_info *) info_buffer;

    // Process the request response
    err = process_request_response(response->status);
    if(err < 0) {
        printf("No successful response, aborting\n");
        return -1;
    }

    // Copy the response data to the info struct
    info->sample_size = response->sample_size;
    info->sample_rate = response->sample_rate;
    info->channels = response->channels;
    info->time_per_packet = response->time_per_packet;
    info->status = response->status;
    strncpy(info->filename, response->filename, strlen(response->filename));

    return 0;
}

/** Create a request packet to send to the server
 * @param argc      the amount of command line arguments
 * @param argv      the string array of arguments
 * @param request   a pointer to the request packet
 *
 * @return 0 on success, <0 otherwise
 */
int create_request (int argc, char **argv, struct request_packet *request) {
    // Verify the size of the arguments
    if(strlen(argv[2]) > FILESIZE_MAX - 1) {
        printf("Filename is too long (%lu characters), max %d characters\n", strlen(argv[2]), FILESIZE_MAX - 1);
        return -1;
    }

    if(argc > 3 && strlen(argv[3]) > FILESIZE_MAX - 1) {
        printf("Library filename is too long (%lu characters), max %d characters\n", strlen(argv[3]), FILESIZE_MAX - 1);
        return -1;
    }

    if(argc > 4 && strlen(argv[4]) > FILESIZE_MAX - 1) {
        printf("Library argument is too long (%lu characters), max %d characters\n", strlen(argv[4]), FILESIZE_MAX - 1);
        return -1;
    }

    // Store the information in the request packet
    strncpy(request->filename, argv[2], strlen(argv[2]));
    request->filename[strlen(argv[2])] = '\0';

    if(argc > 3) {
        strncpy(request->libname, argv[3], strlen(argv[3]));
        request->libname[strlen(argv[3])] = '\0';
    } else {
        strncpy(request->libname, NONE, strlen(NONE));
        request->libname[strlen(NONE)] = '\0';
    }

    if(argc > 4) {
        strncpy(request->libarg, argv[4], strlen(argv[4]));
        request->libarg[strlen(argv[4])] = '\0';
    } else {
        strncpy(request->libarg, NONE, strlen(NONE));
        request->libarg[strlen(NONE)] = '\0';
    }

    return 0;
}

int main (int argc, char *argv [])
{
	int server_fd, audio_fd, err;
	char *ip;
	struct sockaddr_in server;
	struct audio_info info;
	struct request_packet request;

    // trap Ctrl^C signals
    signal( SIGINT, sigint_handler );

	// parse arguments
	if (argc < 3 || argc > 5){
		printf ("error : called with incorrect number of parameters\nusage : %s <server_name/IP> <filename> [<filter> [filter_option]]\n", argv[0]) ;
		return -1;
	}

	err = create_request(argc, argv, &request);
	if(err < 0) {
	    printf("Failed to create request\n");
	    return -1;
	}

	ip = resolve_hostname(argv[1]);

    server_fd = setup_socket(ip, PORT, &server);
    if(server_fd < 0)
        return -1;

    err = send_request(server_fd, &server, &request, &info);
    if(err < 0) {
        printf("Failed to request file\n");
        return -1;
    }

    audio_fd = open_output(&info);
    if(audio_fd < 0) {
        printf("Failed to open output\n");
        return -1;
    }

	err = parse_stream(server_fd, audio_fd, info.time_per_packet);
    if(err < 0) {
        printf("Failed to parse stream\n");
        return -1;
    }

	if (audio_fd >= 0)
		close(audio_fd);
	if (server_fd >= 0)
		close(server_fd);

	return 0 ;
}

