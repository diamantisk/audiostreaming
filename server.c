

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

// Global desciptors for the sigint_handler
int data_fd_g;
int client_fd_g;
void *lib_g;

// This value allows the time between sending packets to be decreased, preventing
// small silent gaps on the client side in case of a long network travel time.
// Values around 0.95-0.97 allow for sufficient padding but not overflowing the client.
float stream_padding = 0.95;

/** Attempt graceful close when Ctrl^C is pressed
 *
 * @param sigint the type of signal received
 */
void sigint_handler(int sigint)
{
    if (!breakloop){
        breakloop=1;
        printf("\tSIGINT catched. Please wait to let the server close gracefully.\nTo close hard press Ctrl^C again.\n");

        if(data_fd_g)
            close(data_fd_g);
        if(client_fd_g)
            close(client_fd_g);
        if(lib_g)
            dlclose(lib_g);

        exit(0);
    }
    else{
        printf ("SIGINT occurred, exiting hard... please wait\n");
        exit(-1);
    }
}

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

/** Send a success response in the form of an audio info packet to the client
 *
 * @param client_fd         the client descriptor
 * @param client            the client socket
 * @param datafile          the name of the datafile
 * @param sample_size       the sample size
 * @param sample_rate       the sample rate
 * @param channels          the number of channels
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

/** Send a failure response in the form of an audio info packet to the client
 *
 * @param client_fd     the client descriptor
 * @param client        the client socket
 * @param status        the status of the failure
 *
 * @return  0 on success, <0 otherwise
 */
int send_request_response_failure(int client_fd, struct sockaddr_in *client, enum flag status) {
    int err;
    struct audio_info info;
    info.status = status;

    err = sendto(client_fd, &info, sizeof(info), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
    if(err < 0) {
        perror("Error sending request response");
        return -1;
    }

    return 0;
}

/** Verify whether a file exists on disk and send an error packet in case it does not
 *
 * @param datafile      the name of the datafile
 * @param client_fd     the client descriptor
 * @param client        the client socket
 *
 * @return  0 on success, or -1 in case of an interrupt
 */
int verify_file_existence(char *datafile, int client_fd, struct sockaddr_in *client) {
    int err;

    if(access(datafile, F_OK) == -1) {
        printf("Error: datafile not found, sending FILE_NOT_FOUND\n");
        err = send_request_response_failure(client_fd, client, FILE_NOT_FOUND);
        if(err < 0)
            perror("Error sending request response");

        return -1;
    }

    return 0;
}

/** Open the input file
 *
 * @param datafile      the name of the datafile
 * @param client_fd     the client descriptor
 * @param client        the client socket
 * @param sample_rate   a pointer to an int which stores the sample rate
 * @param sample_size   a pointer to an int which stores the sample size
 * @param channels      a pointer to an int which stores the number of channels
 *
 * @return  0 on success, or -1 in case of an interrupt
 */
int open_input(char *datafile, int client_fd, struct sockaddr_in *client, int *sample_rate, int *sample_size, int *channels) {
    int data_fd, err;

    data_fd = aud_readinit(datafile, sample_rate, sample_size, channels);
    if (data_fd < 0) {
        printf("Error: failed to open input, sending FAILURE\n");

        err = send_request_response_failure(client_fd, client, FAILURE);
        if(err < 0)
            perror("Error sending request response");

        return -1;
    }

    data_fd_g = data_fd;

    return data_fd;
}

/** Calculate the time required to write a packet to audio
 *
 * @param sample_size   the sample size
 * @param sample_rate   the sample rate
 * @param channels      the number of channels
 * @param buffer_size   the size of the buffer in the packet
 *
 * @return  the time required to write a packet to audio
 */
long calculate_time_per_packet(int sample_size, int sample_rate, int channels, int buffer_size) {
    float bit_rate = sample_size * sample_rate * channels;

    return 8000000000 * ((float) buffer_size / (float) bit_rate);
}

/** Stream audio packets to the client
 *
 * @param client_fd         the client descriptor
 * @param audio_fd          the audio descriptor
 * @param client            the client socket
 * @param lib_requested     Whether a library is requested or not
 * @param time_per_packet   the time required to write a packet to audio
 * @param filter            a pointer to the filter function if aplicable
 *
 * @return  0 on success, <0 otherwise
 */
int stream_data(int client_fd, int data_fd, struct sockaddr_in *client, void *lib, long time_per_packet, server_filter filter) {
	int audiobytesread, err;
    struct audio_packet packet;

    int seq = 0;
    srand(time(NULL));

    audiobytesread = read(data_fd, packet.buffer, BUFSIZE);
    while (audiobytesread > 0) {
        packet.seq = seq;
        packet.audiobytesread = audiobytesread;

        if(filter) {
            printf("[stream_data] lib: %p\n", filter);
            filter(&packet);
        }

        err = sendto(client_fd, &packet, sizeof(struct audio_packet), 0, (struct sockaddr*) client, sizeof(struct sockaddr_in));
        if(err < 0) {
            perror("Error sending packet");
            return 1;
        }

        seq ++;

        // Only sleep if this is not the final packet
        if(audiobytesread == BUFSIZE) {
            nsleep(time_per_packet * stream_padding);
        }

        audiobytesread = read(data_fd, packet.buffer, BUFSIZE);
    }

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
                printf("Filter option %s not available for library %s, sending LIBRARY_ARG_NOT_ALLOWED\n", libarg, libname);
                err = send_request_response_failure(client_fd, client, LIBRARY_ARG_NOT_ALLOWED);
                if(err < 0)
                    perror("Error sending request response");
        }
    }

    return err;
}

/** Open a library
 * @param libname       the name of the library
 * @param libarg        the argument of the library
 * @param lib           A pointer to store the resulting library in
 * @param client_fd     the client socket
 * @param client        the client pointer
 *
 * @return  A pointer to the library on success, NULL otherwise
 */
void *open_lib(char *libname, char *libarg, void *lib, int client_fd, struct sockaddr_in *client) {
    int err;

    lib = dlopen(libname, RTLD_NOW);
    lib_g = lib;

    if(lib == NULL) {
        printf("Failed to load library: %s, sending LIBRARY_NOT_FOUND\n", dlerror());

        err = send_request_response_failure(client_fd, client, LIBRARY_NOT_FOUND);
        if(err < 0)
            perror("Error sending request response");

        return NULL;
    }

    err = verify_libarg(lib, libname, libarg, client_fd, client);
    if(err < 0) {
        dlclose(lib);
        return NULL;
    }

    return lib;
}

/** Process an opened library by seeing which kind of filter it applies
 *
 * @param lib           A pointer to the library
 * @param client_fd     the client descriptor
 * @param client        the client pointer
 * @param sample_rate   the sample rate
 * @param sample_size   the sample size
 * @param channels      the amount of channels
 * @param data_fd       the data descriptor
 * @param request       a pointer to the request packet
 * @return  0 on success, <0 otherwise
 */
int process_lib(void *lib, int client_fd, struct sockaddr_in *client, int *sample_rate, int *sample_size, int *channels, int data_fd, struct request_packet *request) {
    int err;

    if(lib == NULL) {
        // No library to process
        return 0;
    }

    // alter_speed
    server_alter_sample_rate alter_sample_rate = dlsym(lib, "alter_sample_rate");
    if(alter_sample_rate) {
        *sample_rate = alter_sample_rate(*sample_rate);
        return 0;
    }

    // reverse
    server_reverse reverse = dlsym(lib, "reverse");
    if(reverse) {
        err = reverse(request->filename, *channels);
        if(err < 0) {
            printf("Failed to apply reverse effect, sending FAILURE\n");
            err = send_request_response_failure(client_fd, client, FAILURE);
            if(err < 0)
                perror("Error sending request response");

            return err;
        }

        // Open the reversed file while closing the original file
        close(data_fd);
        data_fd = open_input(request->filename, client_fd, client, sample_rate, sample_size, channels);
        if(data_fd < 0) {
            printf("Failed to open reversed file\n");
            dlclose(lib);
            return data_fd;
        }

        return 0;
    }

    return 0;
}

/** Process a request made by a client
 *
 * @param client_fd     the client descriptor
 * @param client        the client socket
 * @param datafile      the name of the datafile
 * @param request       a pointer to the request packet to store the data in
 *
 * @return  0 on success, <0 otherwise
 */
int process_request(int client_fd, struct sockaddr_in *client, struct request_packet *request) {
    int data_fd, err;
    int channels, sample_size, sample_rate;
    long time_per_packet;

    void *lib = NULL;
    server_filter filter = NULL;

    // Verify file existence
    err = verify_file_existence(request->filename, client_fd, client);
    if(err < 0) {
        return err;
    }

    // Open the requested library if applicable
    if(strcmp(request->libname, NONE) != 0) {
        lib = open_lib(request->libname, request->libarg, lib, client_fd, client);
        if(lib == NULL)
            return -1;
    }

    // Open the requested file
    data_fd = open_input(request->filename, client_fd, client, &sample_rate, &sample_size, &channels);
    if(data_fd < 0) {
        return data_fd;
    }

    // Process the library if applicable
    err = process_lib(lib, client_fd, client, &sample_rate, &sample_size, &channels, data_fd, request);
    if(err < 0) {
        printf("Failed to process library information\n");
        return err;
    }

    // The filter pointer will pass along as NULL if the library does not provide a filter, so no checks are needed)
    filter = dlsym(lib, "filter");

    time_per_packet = calculate_time_per_packet(sample_size, sample_rate, channels, BUFSIZE);

    // Send a successful response with audio information
    err = send_request_response_success(client_fd, client, request->filename, sample_size, sample_rate, channels, time_per_packet);
    if(err < 0)
        return err;

    // Start streaming
    err = stream_data(client_fd, data_fd, client, lib, time_per_packet, filter);
    if(err < 0)
        printf("Error while streaming\n");

    return err;
}

/** Open a request made by a client and store the library info in a request packet
 *
 * @param request_buffer    the buffer received from the request
 * @param request           a pointer to the request packet to store the data in
 */
void open_request(char *request_buffer, struct request_packet *request) {
    struct request_packet *received = (struct request_packet *) request_buffer;

    // Filename
    strncpy(request->filename, received->filename, strlen(received->filename));
    request->filename[strlen(received->filename)] = '\0';

    // Library name
    strncpy(request->libname, received->libname, strlen(received->libname));
    request->libname[strlen(received->libname)] = '\0';

    // Library argument
    strncpy(request->libarg, received->libarg, strlen(received->libarg));
    request->libarg[strlen(received->libarg)] = '\0';
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

    client_fd_g = client_fd;

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

/** The main loop, continuously waiting for clients
 *
 * @param argc  The amount of command line parameters
 * @param argv  The command line parameters
 * @return  0 on success, <0 otherwise
 */
int main (int argc, char **argv)
{
	int client_fd, err;
	char request_buffer[sizeof(struct request_packet)];
	socklen_t flen;
	struct sockaddr_in client;

    // trap Ctrl^C signals
    signal(SIGINT, sigint_handler );

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

        // Print a recognizable start and end of the streaming process
        printf("----- Request received from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        open_request(request_buffer, &request);
        err = process_request(client_fd, &client, &request);

		printf("----- Request processed from %s:%d: ", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		(err == 0) ? (printf("Success")) : (printf("Failure"));
		printf("\n\n");
	}

	return 0;
}
