/* server.c
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <netinet/in.h>
#include <time.h>

#include "library.h"
#include "audio.h"
#include "packet.h"

/// a define used for the copy buffer in stream_data(...)

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

//struct __attribute__((packed)) audio_info {
//    int channels, sample_size, sample_rate, bit_rate;
//    char filename[FILENAME_MAX];
//};

int nsleep(long nanoseconds) {
    struct timespec req, rem;

    if(nanoseconds >= 1000000000) {
        printf("More than a sec: %ld\n", nanoseconds);
        req.tv_sec = (int) (nanoseconds / 1000000000);
        req.tv_nsec = (nanoseconds - ((long) req.tv_sec * 1000000000));
    } else {
        req.tv_sec = 0;
        req.tv_nsec = nanoseconds;
    }

    return nanosleep(&req, &rem);
}

/// stream data to a client. 
///
/// This is an example function; you do *not* have to use this and can choose a different flow of control
///
/// @param fd an opened file descriptor for reading and writing
/// @return returns 0 on success or a negative errorcode on failure
int stream_data(int client_fd, struct sockaddr_in *client, char *filename)
{
    printf("Start streaming... const: %d\n", FILENAME_MAX);

	int data_fd, err;
	int channels, sample_size, sample_rate, bit_rate;
	server_filterfunc pfunc;
	char *datafile, *libfile;
	char buffer[BUFSIZE];

	long time_per_packet;   // Nanoseconds
	
	// TO IMPLEMENT
	// receive a control packet from the client 
	// containing at the least the name of the file to stream and the library to use
	{
		datafile = strdup(filename);
		libfile = NULL;

//		sample_size = 4;
//        sample_rate = 44100;
//        channels = 2;
	}

//	sample_size = 8;
//    sample_rate = 11025;
//    channels = 1;
//
	// open input
    data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels);
	if (data_fd < 0){
		printf("failed to open datafile %s, skipping request\n",datafile);
		return -1;
	}
	printf("opened datafile %s\n",datafile);

    // debug
//    sample_rate = sample_rate * 5;
//    bit_rate = bit_rate * 8;

	bit_rate = sample_size * sample_rate * channels;
	time_per_packet = 1000000000 * ((float) BUFSIZE / (float) bit_rate);
	time_per_packet = time_per_packet * 8;

    // create an acknowledgement packet
    struct audio_info info;
    info.sample_rate = sample_rate;
    info.sample_size = sample_size;
    info.channels = channels;
    strncpy(info.filename, filename, FILENAME_MAX);

    printf("info sample size: %d, info filename: %s, sizeof(info): %d\n", info.sample_size, info.filename, sizeof(info));


    printf("Sending info (%d bytes): %s\n", sizeof(info), info.filename);
    err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
    if(err < 0) {
        perror("Error sending info packet");
        return 1;
    }

	// optionally open a library
	if (libfile){
		// try to open the library, if one is requested
		pfunc = NULL;
		if (!pfunc){
			printf("failed to open the requested library. breaking hard\n");
			return -1;
		}
		printf("opened libraryfile %s\n",libfile);
	}
	else{
		pfunc = NULL;
		printf("not using a filter\n");
	}
	
	// TO IMPLEMENT : optionally return an error code to the client if initialization went wrong
	
	// start streaming


	{
		int bytesread, bytesmod;

		/** Client stuff here START **/
        // open output
//        int audio_fd;
//        audio_fd = aud_writeinit(sample_rate, sample_size, channels);
//        if (audio_fd < 0){
//            printf("error: unable to open audio output.\n");
//            return -1;
//        }
        /** Client stuff here END **/

        printf("sample_rate: %d, sample_size: %d, channels: %d, bit_rate: %d\n", sample_rate, sample_size, channels, bit_rate);
        printf("time_per_packet: %ld nanoseconds (%f seconds)\n", time_per_packet, (float) time_per_packet / 1000000000);

//        return 0;

//        int i = 0;
//        while(i < 10) {
//            printf("hi %d\n", i ++);
//            nsleep(time_per_packet);
//        }
//
//		return 0;

        int i = 0;

//        return 0;

//        nsleep(1 * 1000000000);

		bytesread = read(data_fd, buffer, BUFSIZE);
		while (bytesread > 0){
//		    printf("Read %d bytes\n", bytesread);
			// you might also want to check that the client is still active, whether it wants resends, etc..
			
			// edit data in-place. Not necessarily the best option
			if (pfunc)
				bytesmod = pfunc(buffer,bytesread);

//			write(client_fd, buffer, bytesmod);

            if(i > -1) {
                err = sendto(client_fd, buffer, bytesread, 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
                if(err < 0) {
                    perror("Error sending packet");
                    return 1;
                }
            } else {
                printf("Skipping...\n");
            }

			printf("Sent %d bytes (I'm a server, i: %d)\n", bytesread, i++);
			i--;

            nsleep(time_per_packet);
//            printf("I just slept %ld miliseconds (%ld nanoseconds)\n", time_per_packet / 1000000, time_per_packet);

            /** Client stuff here START **/
//            write(audio_fd, buffer, bytesread);

			bytesread = read(data_fd, buffer, BUFSIZE);
		}
	}

	// TO IMPLEMENT : optionally close the connection gracefully 	
	
//	if (client_fd >= 0)
//		close(client_fd);
	if (data_fd >= 0)
		close(data_fd);
	if (datafile)
		free(datafile);
	if (libfile)
		free(libfile);
	
	return 0;
}

/// unimportant: the signal handler. This function gets called when Ctrl^C is pressed
void sigint_handler(int sigint)
{
	if (!breakloop){
		breakloop=1;
		printf("SIGINT catched. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");
	}
	else{
       		printf ("SIGINT occurred, exiting hard... please wait\n");
		exit(-1);
	}
}

/// the main loop, continuously waiting for clients
int main (int argc, char **argv)
{
	printf ("SysProg network server\n");
	printf ("handed in by tsg280\n");

	int client_fd, err;
	char buf[FILENAME_MAX];
	socklen_t flen;
	struct sockaddr_in client;
	
	// TODO re-enable for submission
	// signal(SIGINT, sigint_handler );	// trap Ctrl^C signals

	client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(client_fd < 0) {
		perror("Error initializing socket");
		return 1;
	}

	client.sin_family = AF_INET;
	client.sin_port = htons(PORT);
	client.sin_addr.s_addr = htonl(INADDR_ANY);

	flen = sizeof(struct sockaddr_in);

	err = bind(client_fd, (struct sockaddr *) &client, sizeof(struct sockaddr_in));
	if(err < 0) {
		perror("Error binding socket");
		return 1;
	}

	printf("Bound port %d\n", PORT);

//	stream_data(client_fd, &client, "long.wav");
//
//    return 0;
	
	while (!breakloop){
		// TO IMPLEMENT:
		// 	wait for connections
		// 	when a client connects, start streaming data (see the stream_data(...) prototype above)

		printf("Waiting...\n");
		printf("BUFSIZE: %d\n", BUFSIZE);

		err = recvfrom(client_fd, buf, FILENAME_MAX, 0, (struct sockaddr*) &client, &flen);
		if(err < 0) {
			perror("Error receiving packet");
			return 1;
		}

		char filename[err + 1];
        strncpy(filename, buf, err);
        filename[err] = '\0';

		printf("Received %d bytes from port %d: %s (bufsize: %d, filename size: %d)\n", err, ntohs(client.sin_port), filename, sizeof(buf), strlen(filename));

//		printf("Sending ack (%d bytes): %s\n", err, ack);
//		err = sendto(client_fd, ack, err, 0, (struct sockaddr*) &client, sizeof(struct sockaddr_in));
//        if(err < 0) {
//            perror("Error sending packet");
//            return 1;
//        }

		stream_data(client_fd, &client, filename);

		printf("Done streaming\n");

//		printf("Received %d bytes from host %s port %d: %s\n", err, inet_ntoa(client.sin_addr), ntohs(client.sin_port), buf);
	}

	return 0;
}