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
	char buf[BUFSIZE];

	// for ack
	fd_set read_set;
	struct timeval timeout;

	// debug
	struct timeval start, end;
	double rtt;

	char *ip;
	struct sockaddr_in server;
	
	signal( SIGINT, sigint_handler );	// trap Ctrl^C signals
	
	// parse arguments
	if (argc < 3){
		printf ("error : called with incorrect number of parameters\nusage : %s <server_name/IP> <filename> [<filter> [filter_options]]]\n", argv[0]) ;
		return -1;
	}

	char *filename = argv[2];

	if(strlen(filename) > FILESIZE_MAX) {

	}

//	printf("filename: %s (size: %d)\n", filename, strlen(filename));
	
	// TO IMPLEMENT : open input
	/* Working on this */
	ip = resolve_hostname(argv[1]);

	printf("ip: %s\n", ip);
	server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (server_fd < 0){
		perror("Error initializing socket");
		return -1;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = inet_addr(ip);

	server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(server_fd < 0) {
            perror("Error initializing socket");
            return 1;
    }

	if (server_fd < 0){
		printf("error: unable to connect to server.\n");
		return -1;
	}

	// TO IMPLEMENT
	// send the requested filename and library information to the server
	// and wait for an acknowledgement. Or fail if the server returns an errorcode

	// OWN STUFF
	// send packet with file name

//	char ack[strlen(filename)];
//	strncpy(ack, filename, strlen(filename));
	err = sendto(server_fd, filename, strlen(filename), 0, (struct sockaddr*) &server, sizeof(struct sockaddr_in));
	if(err < 0) {
		perror("Error sending packet");
		return 1;
	}

	printf("Sent: %s (length: %lu)\n", filename, sizeof(filename));

	printf("Waiting for ack\n");

	FD_ZERO(&read_set);
	FD_SET(server_fd, &read_set);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	nb = select(server_fd + 1, &read_set, NULL, NULL, &timeout);

	int bytesread;
	char info_buffer[sizeof(struct audio_info)];
	printf("Made info_buffer with size: %lu\n", sizeof(struct audio_info));

	if(nb < 0) {
	    perror("Error waiting for timeout");
	} else
    if(nb == 0) {
        printf("Acknowledgement packet was lost, assuming that server is busy\n");
        return 1;
    } else
    if(FD_ISSET(server_fd, &read_set)) {
        bytesread = read(server_fd, info_buffer, BUFSIZE);
        printf("Received %d bytes: %s\n", bytesread, info_buffer);
//        return 0;
    }

    struct audio_info *info = (struct audio_info *) info_buffer;

    sample_size = info->sample_size;
    sample_rate = info->sample_rate;
    channels = info->channels;

    printf("sample_rate: %d, sample_size: %d, channels: %d\n", info->sample_rate, info->sample_size, info->channels);
    printf("sample_rate: %d, sample_size: %d, channels: %d\n", sample_rate, sample_size, channels);

//    printf("info size: %d (BUFSIZE: %d)\n", sizeof(info), BUFSIZE);
//    printf("info_buffer size: %d (BUFSIZE: %d)\n", sizeof(info_buffer), BUFSIZE);

//	bytesread = read(server_fd, buf, BUFSIZE);

	{
//		sample_size = 8;
//        sample_rate = 11025;
//        channels = 1;
	}

//	return 0;

	// open output
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
		printf("not using a filter\n");
	}

	// start receiving data
	{
//		int bytesread, bytesmod;
//		char *modbuffer;

		printf("Reading... BUFSIZE: %d\n", BUFSIZE);

		bytesread = read(server_fd, buf, BUFSIZE);
		while (bytesread == BUFSIZE){
		    printf("Read %d bytes (I'm a client)\n", bytesread);
			// edit data in-place. Not necessarily the best option
			if (pfunc)
//				modbuffer = pfunc(buf,bytesread,&bytesmod);
//			write(audio_fd, modbuffer, bytesmod);

            gettimeofday(&start, NULL);

            write(audio_fd, buf, bytesread);

            gettimeofday(&end, NULL);

            rtt = (end.tv_sec - start.tv_sec);
            rtt += (end.tv_usec - start.tv_usec) / 1000000.0;

//            printf("That took %f seconds\n", rtt);

			bytesread = read(server_fd, buf, BUFSIZE);
		}
	}

	printf("Done\n");

	if (audio_fd >= 0)
		close(audio_fd);
	if (server_fd >= 0)
		close(server_fd);

	return 0 ;
}

