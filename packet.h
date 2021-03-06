#ifndef _PACKET_H_
#define _PACKET_H_

#define BUFSIZE 1024
#define PORT 12345
#define FILESIZE_MAX 101

struct __attribute__((packed)) audio_info {
    int sample_size;
    int sample_rate;
    int channels;
    long time_per_packet;   // Nanoseconds
    char filename[FILESIZE_MAX];
    enum flag {SUCCESS, FILE_NOT_FOUND, LIBRARY_NOT_FOUND, LIBRARY_ARG_NOT_ALLOWED, LIBRARY_ARG_REQUIRED, FAILURE} status;
};

struct __attribute__((packed)) request_packet {
    char filename[FILESIZE_MAX];
    char libname[FILESIZE_MAX];
    char libarg[FILESIZE_MAX];
};

struct __attribute__((packed)) audio_packet {
    int seq;
    int audiobytesread;
    char buffer[BUFSIZE];
};

#endif /* _PACKET_H_ */