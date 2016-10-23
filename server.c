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
#include <dlfcn.h>

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

/** Send an audio info packet to the client
 *
 * @param client_fd  the client descriptor
 * @param client  the client socket
 * @param datafile  the name of the datafile
 * @param sample_size   the sample size
 * @param sample_rate   the sample rate
 * @param channels   the number of channels
 * @param time_per_packet   the time required to write a packet to audio
 *
 * @return  0 on success, <0 otherwise
 */
int send_request_response_success(int client_fd, struct sockaddr_in *client, char *datafile, int sample_size, int sample_rate, int channels, long time_per_packet) {
    int err;
    struct audio_info info;

    info.sample_rate = sample_rate;
    info.sample_size = sample_size;
    info.channels = channels;
    info.time_per_packet = time_per_packet;
    info.status = SUCCESS;
    strncpy(info.filename, datafile, strlen(datafile));

    err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
    if(err < 0) {
        perror("Error sending initial packet");
        return -1;
    }

    return 0;
}

int send_request_response_failure(int client_fd, struct sockaddr_in *client, enum flag status) {
    int err;
    struct audio_info info;
    info.status = status;

    if(status == FILE_NOT_FOUND) {
        printf("yay we got failure\n");
    }

    err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
    if(err < 0) {
        perror("Error sending request response");
        return -1;
    }

    return 0;
}

/** Verify whether a file exists on disk and send an error packet in case it does not
 *
 * @param datafile  the name of the datafile
 * @param client_fd  the client descriptor
 * @param client  the client socket
 *
 * @return  0 on success, or -1 in case of an interrupt
 */
int verify_file_existence(char *datafile, int client_fd, struct sockaddr_in *client) {
    int err;
    // Check for file existence
    if(access(datafile, F_OK) == -1) {
        printf("Error: datafile not found, sending FILE_NOT_FOUND\n");
        err = send_request_response_failure(client_fd, client, FILE_NOT_FOUND);
        if(err < 0) {
            perror("Error sending request response");
        }

        return -1;
    }

    return 0;
}

/** Open the input file
 *
 * @param datafile  the name of the datafile
 * @param client_fd  the client descriptor
 * @param client  the client socket
 * @param sample_rate  a pointer to an int which stores the sample rate
 * @param sample_size  a pointer to an int which stores the sample size
 * @param channels  a pointer to an int which stores the number of channels
 *
 * @return  0 on success, or -1 in case of an interrupt
 */
int open_input(char *datafile, int client_fd, struct sockaddr_in *client, int *sample_rate, int *sample_size, int *channels) {
    int data_fd, err;

     data_fd = aud_readinit(datafile, sample_rate, sample_size, channels);
    if (data_fd < 0) {
        printf("Error: failed to open input, sending FAILURE\n");

        err = send_request_response_failure(client_fd, client, FAILURE);
        if(err < 0) {
            perror("Error sending request response");
        }

        return -1;
    }

    return data_fd;
}

/** Calculate the time required to write a packet to audio
 *
 * @param sample_size   the sample size
 * @param sample_rate   the sample rate
 * @param channels   the number of channels
 * @param buffer_size   the size of the buffer in the packet
 *
 * @return  the time required to write a packet to audio
 */
long calculate_time_per_packet(int sample_size, int sample_rate, int channels, int buffer_size) {
    float bit_rate = sample_size * sample_rate * channels;
    return 8000000000 * ((float) buffer_size / (float) bit_rate); //453514720
}

/** Stream audio packets to the client
 *
 * @param client_fd  the client descriptor
 * @param audio_fd  the audio descriptor
 * @param client  the client socket
 * @param datafile  the name of the datafile
 * @param time_per_packet   the time required to write a packet to audio
 *
 * @return  0 on success, <0 otherwise
 */
int stream_data(int client_fd, int data_fd, struct sockaddr_in *client, char *datafile, long time_per_packet)
{
	int audiobytesread, err;
    struct audio_packet packet;

    // TODO debug
//    printf("sample_rate: %d, sample_size: %d, channels: %d, bit_rate: %d\n", sample_rate, sample_size, channels, bit_rate);
//    printf("time_per_packet: %ld nanoseconds (%f seconds)\n", time_per_packet, (float) time_per_packet / 1000000000);
    int i = 0;
    int seq = 0;
    int seqtemp = 0;
    srand(time(NULL));


    audiobytesread = read(data_fd, packet.buffer, BUFSIZE);
    while (audiobytesread > 0){
        i = 0;

        seqtemp = seq;

        if(seq % 5 == 4) {
//            seq = 1337;
        }

        if(i % 2 == 0) {
            packet.seq = seq;
            packet.audiobytesread = audiobytesread;
            err = sendto(client_fd, &packet, sizeof(struct audio_packet), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
            if(err < 0) {
                perror("Error sending packet");
                return 1;
            }
            printf("Sent %d bytes of audio (packet number: %d)\n", audiobytesread, seq);
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
        if(audiobytesread == BUFSIZE) {
            nsleep(time_per_packet);
        }

        audiobytesread = read(data_fd, packet.buffer, BUFSIZE);

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

/** Open a library
 * @param libname   the name of the library
 * @param libarg    the argument of the library
 * @param lib       A pointer to store the resulting library in
 *
 * @return  0 on success, <0 otherwise
 */
int open_lib(char *libname, char *libarg, void *lib, int client_fd, struct sockaddr_in *client) {
    int err;
    server_verify verify_arg;

    lib = dlopen(libname, RTLD_NOW);

    if(lib == NULL) {
        printf("Failed to load library: %s, sending LIBRARY_NOT_FOUND\n", dlerror());

        err = send_request_response_failure(client_fd, client, LIBRARY_NOT_FOUND);
        if(err < 0) {
            perror("Error sending request response");
        }

        return -1;
    }

    verify_arg = dlsym(lib, "verify_arg");
    if (verify_arg == NULL) {
        printf("Failed to load symbol: %s, sending FAILURE\n", dlerror());

        err = send_request_response_failure(client_fd, client, FAILURE);
        if(err < 0) {
            perror("Error sending request response");
        }

        return -1;
    }

    err = verify_arg(libarg);
    if(err < 0) {
        printf("Filter option %s not available for library %s, sending LIBRARY_ARG_NOT_FOUND\n", libarg, libname);

        err = send_request_response_failure(client_fd, client, LIBRARY_ARG_NOT_FOUND);
        if(err < 0) {
            perror("Error sending request response");
        }

        return -1;
    }

    return 0;
}

/** Open a request made by a client and store the library info in a request packet
 *
 * @param request_buffer    the buffer received from the request
 * @param request   a pointer to the request packet to store the data in
 */
void open_request(char *request_buffer, struct request_packet *request) {
    struct request_packet *received = (struct request_packet *) request_buffer;

    strncpy(request->filename, received->filename, strlen(received->filename));
    request->filename[strlen(received->filename)] = '\0';

    strncpy(request->libname, received->libname, strlen(received->libname));
    request->libname[strlen(received->libname)] = '\0';

    strncpy(request->libarg, received->libarg, strlen(received->libarg));
    request->libarg[strlen(received->libarg)] = '\0';
}

/** Process a request made by a client
 *
 * @param client_fd  the client descriptor
 * @param client  the client socket
 * @param datafile  the name of the datafile
 *
 * @return  0 on success, <0 otherwise
 */
int process_request(int client_fd, struct sockaddr_in *client, struct request_packet *request) {
    int data_fd, err;
    int channels, sample_size, sample_rate;
    long time_per_packet;
    void *lib = NULL;

    // Verify file existence
    err = verify_file_existence(request->filename, client_fd, client);
    if(err < 0) {
        return err;
    }

    // Open the requested file
    data_fd = open_input(request->filename, client_fd, client, &sample_rate, &sample_size, &channels);
    if(data_fd < 0) {
        return data_fd;
    }

    // Open the requested library if applicable
    if(strcmp(request->libname, NONE) != 0) {
        err = open_lib(request->libname, request->libarg, lib, client_fd, client);
        if(err < 0) {

        }
    } else {
        printf("No library requested\n");
    }

    time_per_packet = calculate_time_per_packet(sample_size, sample_rate, channels, BUFSIZE);

    err = send_request_response_success(client_fd, client, request->filename, sample_size, sample_rate, channels, time_per_packet);
    if(err < 0) {
        return err;
    }

    return stream_data(client_fd, data_fd, client, request->filename, time_per_packet);
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
	char request_buffer[sizeof(struct request_packet)];
	socklen_t flen;
	struct sockaddr_in client;

	// TODO re-enable for submission
//	 signal(SIGINT, sigint_handler );	// trap Ctrl^C signals

    // lib stuff, it works
    /*char *libfile = "/home/thomas/Documents/sysprog/assignment4/libblank.so";
    char *libarg = "mids";
    void *mylibrary;

    err = open_lib(libfile, mylibrary, libarg);

    return 0;

    server_filterfunc pfunc;

    // optionally open a library
    if (libfile){
        // try to open the library, if one is requested
        pfunc = NULL;

        mylibrary = dlopen(libfile, RTLD_NOW);
        if(mylibrary == NULL) {
            printf("Failed to load library: %s\n", dlerror());
            return -1;
        }

        pfunc = dlsym(mylibrary, "encode");

        if (pfunc == NULL){
            printf("Failed to load symbol: %s\n", dlerror());
            return -1;
        }

        printf("opened libraryfile %s\n",libfile);

        int result = pfunc("hi", 10);
        dlclose(mylibrary);
    }
    else{
        pfunc = NULL;
        printf("not using a filter\n");
    }*/

    client_fd = setup_socket(PORT, &flen, &client);
    if(client_fd < 0) {
        printf("Failed to set up socket\n");
        return -1;
    }

    printf("Listening on port %d...\n\n", PORT);

	while (!breakloop) {

	    struct request_packet request;
	    // Wait for incoming messages
		err = recvfrom(client_fd, request_buffer, FILENAME_MAX, 0, (struct sockaddr*) &client, &flen);
		if(err < 0) {
			perror("Error receiving packet");
			return -1;
		}

        // Print a recognizable start and end of the streaming progress to stdout
        printf("^^^^ Request received from %s:%d ^^^^\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        open_request(request_buffer, &request);
        err = process_request(client_fd, &client, &request);

		printf("---- Request processed from %s:%d: ", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		(err == 0) ? (printf("Success")) : (printf("Failure"));
		printf(" ----\n\n");

//         printf("[main] filename: %s (%ld)\n", request.filename, strlen(request.filename));
//         printf("[main] libname: %s (%ld)\n", request.libname, strlen(request.libname));
//         printf("[main] libarg: %s (%ld)\n", request.libarg, strlen(request.libarg));
	}

	return 0;
}