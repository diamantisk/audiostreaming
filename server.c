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
    long time = 8000000000 * ((float) buffer_size / (float) bit_rate);
    printf("bit_rate: %f, sample size: %d\n", bit_rate, sample_size);
    printf("time: %ld\n", time);
    return 8000000000 * ((float) buffer_size / (float) bit_rate); //453514720 for 11.12k/8bit/1channel
}

// TODO update
/** Stream audio packets to the client
 *
 * @param client_fd  the client descriptor
 * @param audio_fd  the audio descriptor
 * @param client  the client socket
 * @param lib_requested       Whether a library is requested or not
 * @param datafile  the name of the datafile
 * @param time_per_packet   the time required to write a packet to audio
 *
 * @return  0 on success, <0 otherwise
 */
// TODO remove datafile?
int stream_data(int client_fd, int data_fd, struct sockaddr_in *client, void *lib, char *datafile, long time_per_packet, server_filter filter)
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

//    return 0;

    audiobytesread = read(data_fd, packet.buffer, BUFSIZE);
    while (audiobytesread > 0) {
        i = 0;

        seqtemp = seq;

        if(seq % 5 == 4) {
//            seq = 1337;
        }

        /*int n, sample;
        for(n = 0; n < audiobytesread; n++) {
            printf("Before: %d\n", packet.buffer[n]);
            packet.buffer[n] = 140;
            printf("After: %d\n", packet.buffer[n]);
        }*/

        if(i % 2 == 0) {
            packet.seq = seq;
            packet.audiobytesread = audiobytesread;

            // nonsense here
            /*int j = 1;
            while(j < audiobytesread) {
//                if(j % 1000 == 0) {
                    float percent = 0.8;
                    int newvalue = packet.buffer[j] * percent;

                    printf("%d > %d\n", packet.buffer[j], newvalue);
                    packet.buffer[j] += packet.buffer[j - 1] * 0.1;
//                }
//                packet.buffer[j] *= 0.5 ;
                j ++;
            }*/
//            printf("audiobytesread: %d: %s\n", packet.audiobytesread, packet.buffer);

//            while(j < audiobytesread) {
//                if(j % 50 == 0) {
//                    packet.buffer[j] = "0";
//                    printf("Yay j = %d\n", j);
//                }
//
//                j ++;
//            }


//            return 0;

//            if(audiobytesread < BUFSIZE) {
//                for(int n = 0; n < BUFSIZE; n++) {
//                    packet.buffer[n] = 100;
//                }
//            }

            if(filter) {
                printf("[stream] Filter pointer: %p\n", filter);
                filter(&packet);
            }

            err = sendto(client_fd, &packet, sizeof(struct audio_packet), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
            if(err < 0) {
                perror("Error sending packet");
                return 1;
            }
//            printf("Sent %d bytes of audio (packet number: %d)\n", audiobytesread, seq);
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
            nsleep(time_per_packet * 0.95);
            // TODO make concrete
        }

        audiobytesread = read(data_fd, packet.buffer, BUFSIZE);

//        return 0;
    }

	// TO IMPLEMENT : optionally close the connection gracefully
	if (data_fd >= 0)
		close(data_fd);
	if (lib)
		dlclose(lib);

	return 0;
}

/** Verify the argument provided for a library
 *
 * @param lib           the library pointer
 * @param libname       the library name
 * @param libarg        the library argument
 * @param client_fd     the client socket
 * @param client        the client pointer
 * @return              0 on success, <0 otherwise
 */
int verify_libarg(void *lib, char *libname, char *libarg, int client_fd, struct sockaddr_in *client) {
    int err;
    server_verify verify_arg;

    // Check whether the function verify_arg is present in the library
    verify_arg = dlsym(lib, "verify_arg");
    if(verify_arg == NULL) {
        printf("Filter option required for library %s, sending LIBRARY_ARG_REQUIRED\n", libname);

        err = send_request_response_failure(client_fd, client, LIBRARY_ARG_REQUIRED);
        if(err < 0)
            perror("Error sending request response");

        return -1;
    }

    // Check whether the provided argument is valid for the library
    err = verify_arg(libarg);
    if(err < 0) {
        switch(err) {
            case -2:
                printf("Filter option required for library %s, sending LIBRARY_ARG_REQUIRED\n", libname);
                int err = send_request_response_failure(client_fd, client, LIBRARY_ARG_REQUIRED);
                if(err < 0)
                    perror("Error sending request response");
                break;
            default:
                printf("Filter option %s not available for library %s, sending LIBRARY_ARG_NOT_FOUND\n", libarg, libname);
                err = send_request_response_failure(client_fd, client, LIBRARY_ARG_NOT_FOUND);
                if(err < 0)
                    perror("Error sending request response");
                break;
        }
    }

    return err;
}

// TODO update
/** Open a library
 * @param libname   the name of the library
 * @param libarg    the argument of the library
 * @param lib       A pointer to store the resulting library in
 *
 * @return  0 on success, <0 otherwise
 */
void *open_lib(char *libname, char *libarg, void *lib, int client_fd, struct sockaddr_in *client) {
    int err;

    lib = dlopen(libname, RTLD_NOW);

    if(lib == NULL) {
        printf("Failed to load library: %s, sending LIBRARY_NOT_FOUND\n", dlerror());

        err = send_request_response_failure(client_fd, client, LIBRARY_NOT_FOUND);
        if(err < 0) {
            perror("Error sending request response");
        }

        return NULL;
    }

    err = verify_libarg(lib, libname, libarg, client_fd, client);
    if(err < 0) {
        dlclose(lib);
        return NULL;
    }

//    printf("LIB: %p\n", lib);
    return lib;
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

    server_filter filter = NULL;

    int filter_lib_requested = 0;

    // Verify file existence
    err = verify_file_existence(request->filename, client_fd, client);
    if(err < 0) {
        return err;
    }

    // Open the requested library if applicable
    if(strcmp(request->libname, NONE) != 0) {
        lib = open_lib(request->libname, request->libarg, lib, client_fd, client);
        if(lib == NULL) {
            return -1;
        }
    } else {
        printf("No library requested\n");
    }

    // Open the requested file
    data_fd = open_input(request->filename, client_fd, client, &sample_rate, &sample_size, &channels);
    if(data_fd < 0) {
        return data_fd;
    }

    // TODO refactor, seperate function
    if(lib) {
        // alter_speed
        server_alter_sample_rate alter_sample_rate = dlsym(lib, "alter_sample_rate");
        if(alter_sample_rate)
            sample_rate = alter_sample_rate(sample_rate);

        // reverse
        server_reverse reverse = dlsym(lib, "reverse");
        if(reverse) {
            err = reverse(request->filename, channels);
            if(err < 0) {
                printf("Failed to apply reverse effect\n");
                return err;
            }

            // Open the reversed file
            close(data_fd);
            data_fd = open_input(request->filename, client_fd, client, &sample_rate, &sample_size, &channels);
            if(data_fd < 0) {
                printf("Failed to open reversed file\n");
                dlclose(lib);
                return data_fd;
            }
        }

        // stream filters
        filter = dlsym(lib, "filter");
        if(filter) {
            printf("Filter lib!\n");
//            filter_lib_requested = 1;
//            dlclose(lib);
        }
    }

    printf("[process] Filter pointer: %p\n", filter);

    time_per_packet = calculate_time_per_packet(sample_size, sample_rate, channels, BUFSIZE);

    err = send_request_response_success(client_fd, client, request->filename, sample_size, sample_rate, channels, time_per_packet);
    if(err < 0)
        return err;

//    return 0;

    err = stream_data(client_fd, data_fd, client, lib, request->filename, time_per_packet, filter);

    return err;
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
        printf("----- Request received from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        open_request(request_buffer, &request);
        err = process_request(client_fd, &client, &request);

		printf("----- Request processed from %s:%d: ", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		(err == 0) ? (printf("Success")) : (printf("Failure"));
		printf("\n\n");
	}

	return 0;
}
