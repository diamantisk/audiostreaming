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
#include <arpa/inet.h>
#include <time.h>

#include "library.h"
#include "audio.h"
#include "packet.h"

static int breakloop = 0;	///< use this variable to stop your wait-loop. Occasionally check its value, !1 signals that the program should close

/** Title
 *
 * @return
 */

/** Sleep for a specified amount of time
 *
 * @param nanoseconds   The amount of nanoseconds to sleep
 *
 * @return  0 on success, or -1 in case of an interrupt
 */
int nsleep(long nanoseconds) {
    struct timespec req, rem;

    if(nanoseconds >= 1000000000) {
        req.tv_sec = (int) (nanoseconds / 1000000000);
        req.tv_nsec = (nanoseconds - ((long) req.tv_sec * 1000000000));
    } else {
        req.tv_sec = 0;
        req.tv_nsec = nanoseconds;
    }

    return nanosleep(&req, &rem);
}

int verify_file_existence(char *datafile, int client_fd, struct sockaddr_in *client) {
    // Check for file existence
    if(access(datafile, F_OK) == -1) {
        struct audio_info info;
        info.status = FILE_NOT_FOUND;

        printf("Error: datafile not found, sending FILE_NOT_FOUND\n");
        int err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
        if(err < 0) {
            perror("Error sending initial packet");
        }

        return -1;
    }

    return 0;
}

int open_input(char *datafile, int client_fd, struct sockaddr_in *client, int *sample_rate, int *sample_size, int *channels) {
    int data_fd = aud_readinit(datafile, sample_rate, sample_size, channels);
    if (data_fd < 0) {
        struct audio_info info;
        info.status = FAILURE;

        printf("Error: failed to open input, sending FAILURE\n");
        int err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
        if(err < 0) {
            perror("Error sending initial packet");
        }

        return -1;
    }

    return data_fd;
}

int process_audio_info(int sample_size, int sample_rate, int channels, long *time_per_packet) {
    float bit_rate = sample_size * sample_rate * channels;

    long local_time = 8000000000 * ((float) BUFSIZE / bit_rate);
     printf("local time: %ld (BUFSIZE: %d)\n", local_time, BUFSIZE);
     printf("bit_rate: %f\n", bit_rate);

     *time_per_packet = 8000000000 * ((float) BUFSIZE / (float) bit_rate);


     return 0;
}

int process_request(int client_fd, struct sockaddr_in *client, char *datafile) {
    int data_fd, err;
    int channels, sample_size, sample_rate, bit_rate;
    long time_per_packet;
    struct audio_info info;

    err = verify_file_existence(datafile, client_fd, client);
    if(err < 0) {
        return err;
    }

    data_fd = open_input(datafile, client_fd, client, &sample_rate, &sample_size, &channels);
    if(data_fd < 0) {
        return data_fd;
    }

    process_audio_info(sample_size, sample_rate, channels, &time_per_packet);

    printf("process_request: time_per_packet = %ld (%d)\n", time_per_packet, time_per_packet / 1000000);

    return 0;

    // Calculate audio info

    bit_rate = sample_size * sample_rate * channels;
    time_per_packet = 8000000000 * ((float) BUFSIZE / (float) bit_rate);

    // create initial packet
    info.sample_rate = sample_rate;
    info.sample_size = sample_size;
    info.channels = channels;
    info.time_per_packet = time_per_packet;
    info.status = SUCCESS;
    strncpy(info.filename, datafile, FILENAME_MAX);

//    printf("Sending initial packet\n");
//    err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
//    if(err < 0) {
//        perror("Error sending initial packet");
//        return -1;
//    }

    return 0; //stream_data(client_fd, data_fd, client, datafile);
}

// TODO documentation
/// stream data to a client. 
///
/// This is an example function; you do *not* have to use this and can choose a different flow of control
///
/// @param fd an opened file descriptor for reading and writing
/// @return returns 0 on success or a negative errorcode on failure
int stream_data(int client_fd, int data_fd, struct sockaddr_in *client, char *datafile)
{
	int err;
	int bytesread;
	long time_per_packet;   // Nanoseconds


//	server_filterfunc pfunc;
//	char *libfile;
//	char buffer[BUFSIZE];

	// TODO implement
//	libfile = NULL;

//    err = verify_file_existence(datafile, client_fd, client);
//    if(err < 0) {
//        return err;
//    }
//
//    data_fd = open_input(datafile, client_fd, client, &sample_rate, &sample_size, &channels);
//    if(data_fd < 0) {
//        return data_fd;
//    }

//    // Calculate audio info
//	bit_rate = sample_size * sample_rate * channels;
//	time_per_packet = 8000000000 * ((float) BUFSIZE / (float) bit_rate);

//    // create initial packet
//    info.sample_rate = sample_rate;
//    info.sample_size = sample_size;
//    info.channels = channels;
//    info.time_per_packet = time_per_packet;
//    info.status = SUCCESS;
//    strncpy(info.filename, datafile, FILENAME_MAX);

//    printf("Sending initial packet\n");
//    err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
//    if(err < 0) {
//        perror("Error sending initial packet");
//        return -1;
//    }

	// optionally open a library
	/*if (libfile){
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
	}*/

	return 0;


    // TODO debug
//    printf("sample_rate: %d, sample_size: %d, channels: %d, bit_rate: %d\n", sample_rate, sample_size, channels, bit_rate);
//    printf("time_per_packet: %ld nanoseconds (%f seconds)\n", time_per_packet, (float) time_per_packet / 1000000000);
    int i = 0;
    int seq = 0;
    int seqtemp = 0;
    srand(time(NULL));

//    return 0;

    struct audio_packet packet;

    bytesread = read(data_fd, packet.buffer, BUFSIZE);
    while (bytesread > 0){
        i = 0;

        // you might also want to check that the client is still active, whether it wants resends, etc..

        // edit data in-place. Not necessarily the best option
//        if (pfunc)
//            bytesmod = pfunc(buffer,bytesread);
//			write(client_fd, buffer, bytesmod);

        seqtemp = seq;

        if(seq % 5 == 4) {
            seq = 1337;
        }

        if(i % 2 == 0) {
            packet.seq = seq;
            packet.audiobytesread = bytesread;
            err = sendto(client_fd, &packet, sizeof(struct audio_packet), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
            if(err < 0) {
                perror("Error sending packet");
                return 1;
            }
            printf("Sent %d bytes of audio (packet number: %d)\n", bytesread, seq);
            printf("Sizeof(packet): %lu\n", sizeof(struct audio_packet));
            seq ++;
            seqtemp ++;
        } else {
            float usleep = (rand() / (float) RAND_MAX);
            long lsleep = 400000000 * usleep;
//            printf("Skipping... rand: %ld\n", lsleep);
            nsleep(lsleep);
        }

        seq = seqtemp;

        i ++;

        // Only sleep if this is not the final packet
        if(bytesread == BUFSIZE) {
            nsleep(time_per_packet);
        }

        bytesread = read(data_fd, packet.buffer, BUFSIZE);

//        return 0;
    }

	// TO IMPLEMENT : optionally close the connection gracefully 	
	
//	if (client_fd >= 0)
//		close(client_fd);
	if (data_fd >= 0)
		close(data_fd);
//	if (datafile)
//		free(datafile);
//	if (libfile)
//		free(libfile);
	
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

/** Setup the server socket
 *
 * @param port      the port to bind
 * @param flen      a pointer to an integer containing the size of from
 * @param client    a structure where the source address of the datagram will be copied to
 *
 * @return  The socket identifier on succes, <0 otherwise
 */
int setup_socket(int port, socklen_t *flen, struct sockaddr_in *client) {
    int client_fd, err;

    client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(client_fd < 0) {
        perror("Error initializing socket");
        return -1;
    }

    client->sin_family = AF_INET;
    client->sin_port = htons(port);
    client->sin_addr.s_addr = htonl(INADDR_ANY);

    *flen = sizeof(struct sockaddr_in);

    err = bind(client_fd, (struct sockaddr *) client, sizeof(struct sockaddr_in));
    if(err < 0) {
        perror("Error binding socket");
        return -1;
    }

    return client_fd;
}

/// the main loop, continuously waiting for clients
int main (int argc, char **argv)
{
	int client_fd, err;
	char buffer[FILENAME_MAX];
	socklen_t flen;
	struct sockaddr_in client;
	
	// TODO re-enable for submission
//	 signal(SIGINT, sigint_handler );	// trap Ctrl^C signals

    client_fd = setup_socket(PORT, &flen, &client);
    if(client_fd < 0) {
        printf("Failed to set up socket\n");
        return -1;
    }
	
	while (!breakloop){
	    // Wait for incoming messages
		printf("Listening on port %d\n", PORT);
		err = recvfrom(client_fd, buffer, FILENAME_MAX, 0, (struct sockaddr*) &client, &flen);
		if(err < 0) {
			perror("Error receiving packet");
			return -1;
		}

        // Store the filename
		char filename[err + 1];
        strncpy(filename, buffer, err);
        filename[err] = '\0';

        // Print a recognizable start and end of the streaming progress to stdout
        printf("^^^^ Request received - %s:%d - %s ^^^^\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), filename);
//		err = stream_data(client_fd, &client, filename);
        err = process_request(client_fd, &client, filename);
		printf("---- Request processed - %s:%d - %s - ", inet_ntoa(client.sin_addr), ntohs(client.sin_port), filename);
		(err == 0) ? (printf("Success")) : (printf("Failure"));
		printf(" ----\n");
	}

	return 0;
}