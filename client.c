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

#define SERVER_TIMEOUT_SEC 0
#define SERVER_TIMEOUT_USEC 500000  // microseconds
#define SERVER_TIMEOUT_THRESHOLD 5

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

/// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint)
{
    exit(-1);

	if (!breakloop){
		breakloop=1;
		printf("SIGINT catched. Please wait to let the client close gracefully.\nTo close hard press Ctrl^C again.\n");
	}
	else{
       		printf ("SIGINT occurred, exiting hard... please wait\n");
		exit(-1);
	}
}

int receive_packet(fd_set *read_set, int server_fd, struct audio_packet *packet, long sec, long usec) {
    int bytesread, nb;
    struct timeval timeout;
    char packet_buffer[sizeof(struct audio_packet)];

    timeout.tv_sec  = sec;
    timeout.tv_usec = usec;

    // TODO debug
//    printf("sec: %lu, usec: %lu (%lu)\n", sec, usec, usec / 1000000);

    // Set a timeout for the packet
    FD_ZERO(read_set);
    FD_SET(server_fd, read_set);

    nb = select(server_fd + 1, read_set, NULL, NULL, &timeout);
    if(nb < 0) {
        perror("Error waiting for timeout");
        return -1;
    } else
    if(nb == 0) {
//        printf("No reply, assuming that server is down\n");
        return 0;
    } else
    if(FD_ISSET(server_fd, read_set)) {
        bytesread = read(server_fd, packet_buffer, sizeof(struct audio_packet));
        // TODO debug
//        printf("Received %d bytes\n", bytesread);

        // Cast the received buffer to a packet struct and copy the data to the original packet
        struct audio_packet *received_packet = (struct audio_packet *) packet_buffer;
        packet->seq = received_packet->seq;
        packet->audiobytesread = received_packet->audiobytesread;
        memcpy(packet->buffer, received_packet->buffer, packet->audiobytesread);

        return packet->audiobytesread;

//        int eq = memcmp(packet->buffer, received_packet->buffer, packet->bytesread);
//        printf("Buffers are %s (%d)", (eq == 0) ? "equal" : "NOT equal", eq);

//         printf("received buffer: %s\n", received_packet->buffer);
//         printf("copied buffer: %s\n", packet->buffer);

//         printf("Packet number copied: %d\n", packet->seq);


//         packet->seq = received_packet->seq;
//         packet->bytesread = received_packet->bytesread;
//         strncpy(packet->buffer, received_packet->buffer, received_packet->bytesread);

    }

    return bytesread;
}

/** Resolve the hostname of an IP
 *
 * @param name   The hostname
 *
 * @return  The IP on success, <0 otherwise
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

int main (int argc, char *argv [])
{
	int server_fd, audio_fd, err, nb;
	int sample_size, sample_rate, channels;
	client_filterfunc pfunc;
	char *buffer[BUFSIZE];
	fd_set read_set;
	struct timeval timeout;
//	struct timeval start, end;
//	double rtt;
	char *ip;
	struct sockaddr_in server;
	int bytesread, packetbytesread, audiobytesread, packet_timeouts;
	char info_buffer[sizeof(struct audio_info)];
	long time_per_packet;   // Nanoseconds

	signal( SIGINT, sigint_handler );	// trap Ctrl^C signals

	// parse arguments
	if (argc < 3){
		printf ("error : called with incorrect number of parameters\nusage : %s <server_name/IP> <filename> [<filter> [filter_options]]]\n", argv[0]) ;
		return -1;
	}

    // Verify the filename length
	char *filename = argv[2];
	if(strlen(filename) > FILESIZE_MAX) {
        printf("Filename is too long, max %d characters\n", FILESIZE_MAX);
        return -1;
	}

	ip = resolve_hostname(argv[1]);

    // Set up the server socket
	server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (server_fd < 0){
		perror("Error initializing socket");
		return -1;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = inet_addr(ip);

    // Send the filename to the server
    // TODO make packet with library information

    printf("Requesting %s from %s:%d\n", filename, ip, PORT);
	err = sendto(server_fd, filename, strlen(filename), 0, (struct sockaddr*) &server, sizeof(struct sockaddr_in));
	if(err < 0) {
		perror("Error sending packet");
		return -1;
	}

    // Set a timeout for the initial packet
	FD_ZERO(&read_set);
	FD_SET(server_fd, &read_set);
	timeout.tv_sec = SERVER_TIMEOUT_SEC;
	timeout.tv_usec = SERVER_TIMEOUT_USEC;

	nb = select(server_fd + 1, &read_set, NULL, NULL, &timeout);

    // Parse the result of the timeout
	if(nb < 0) {
	    perror("Error waiting for timeout");
	} else
    if(nb == 0) {
        printf("No reply, assuming that server is busy or down\n");
        return -1;
    } else
    if(FD_ISSET(server_fd, &read_set)) {
        bytesread = read(server_fd, info_buffer, BUFSIZE);
        // TODO debug
//        printf("Received %d bytes: %s\n", bytesread, info_buffer);
    }

    // Parse the initial packet
    struct audio_info *info = (struct audio_info *) info_buffer;
    if(info->status == FILE_NOT_FOUND) {
        printf("Server responded: file not found\n");
        return -1;
    } else
    if(info->status == FAILURE) {
        printf("Server responded: failure\n");
        return -1;
    }

    sample_size     = info->sample_size;
    sample_rate     = info->sample_rate;
    channels        = info->channels;
    time_per_packet = info->time_per_packet;

    // TODO debug
//    printf("sample_rate: %d, sample_size: %d, channels: %d, time_per_packet: %lu\n", sample_rate, sample_size, channels, time_per_packet);

	// Open output
	audio_fd = aud_writeinit(sample_rate, sample_size, channels);
	if (audio_fd < 0){
		printf("error: unable to open audio output.\n");
		return -1;
	}

	// open the library on the clientside if one is requested
	// TODO implement
	if (argv[3] && strcmp(argv[3],"")){
		// try to open the library, if one is requested
		pfunc = NULL;
		if (!pfunc){
			printf("failed to open the requested library. breaking hard\n");
			return -1;
		}
		printf("opened libraryfile %s\n",argv[3]);
	}
	else{
		pfunc = NULL;
		printf("Not using a filter\n");
	}

//    printf("Reading... BUFSIZE: %d\n", BUFSIZE);

//int receive_packet(fd_set *read_set, int server_fd, char **buffer, int buffer_size, long milliseconds) {

    // TODO debug
    int i = 0;

    long packet_timeout_sec  = SERVER_TIMEOUT_SEC;
    long packet_timeout_usec = SERVER_TIMEOUT_USEC + (time_per_packet / 1000);
    packet_timeouts = 0;

//    printf("SERVER_TIMEOUT_USEC: %d, time_per_packet nanosec: %lu, time_per_packet / 1000: %lu\n", SERVER_TIMEOUT_USEC, time_per_packet, time_per_packet / 1000);
//    printf("packet_timeout_sec: %lu, packet_timeout_usec: %lu\n", packet_timeout_sec, packet_timeout_usec);

    struct audio_packet packet;
    int seq_expected = 0;

    audiobytesread = receive_packet(&read_set, server_fd, &packet, packet_timeout_sec, packet_timeout_usec);

//    printf("Packet number main: %d\n", packet.seq);
//    printf("Packet buffer main: %s\n", packet.buffer);

    while (audiobytesread == BUFSIZE || audiobytesread == 0){
        if(audiobytesread == 0) {
            if(packet_timeouts == SERVER_TIMEOUT_THRESHOLD) {
                printf("Too many timeouts, aborting\n");
                return -1;
            }
            printf("Waiting for reply (%d)...\n", packet_timeouts);
            packet_timeouts ++;
        } else {
//            printf("Read %d audio bytes (packet %d)\n", packet.audiobytesread, packet.seq);
            if(packet.seq == seq_expected) {
                write(audio_fd, packet.buffer, audiobytesread);
            } else {
                printf("Wrong packet, expected %d but received %d\n", seq_expected, packet.seq);
            }

//            printf("main buffer: %s\n", packet.buffer);

        // edit data in-place. Not necessarily the best option
//        if (pfunc)
//				modbufferfer = pfunc(buffer,bytesread,&bytesmod);
//			write(audio_fd, modbufferfer, bytesmod);

//        gettimeofday(&start, NULL);

            printf("Sizeof(packet.buffer): %d\n", sizeof(packet.buffer));

//            return 0;
        }

//        gettimeofday(&end, NULL);
//        rtt = (end.tv_sec - start.tv_sec);
//        rtt += (end.tv_usec - start.tv_usec) / 1000000.0;

//            printf("That took %f seconds\n", rtt);

//        bytesread = read(server_fd, buffer, BUFSIZE);

        seq_expected ++;
        audiobytesread = receive_packet(&read_set, server_fd, &packet, packet_timeout_sec, packet_timeout_usec);
    }

    // If applicable, write the final bytes
    if(bytesread > 0) {
        // TODO debug
        printf("Read %d audio bytes (packet %d)\n", packet.audiobytesread, packet.seq);
        write(audio_fd, packet.buffer, audiobytesread);
    }

	printf("Done\n");

	if (audio_fd >= 0)
		close(audio_fd);
	if (server_fd >= 0)
		close(server_fd);

	return 0 ;
}

