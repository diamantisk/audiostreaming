#ifndef _PACKET_H_
#define _PACKET_H_

#define BUFSIZE 5000
#define PORT 12345
#define FILESIZE_MAX 100

//extern int PORT, BUFSIZE, FILESIZE_MAX;

struct __attribute__((packed)) audio_info {
    int channels;
    int sample_size;
    int sample_rate;
    long time_per_packet;   // Nanoseconds
    char filename[FILESIZE_MAX];
    enum flag {SUCCESS, FILE_NOT_FOUND, FAILURE} status;
};

struct __attribute__((packed)) audio_packet {
    int seq;
    int audiobytesread;
    char buffer[BUFSIZE];
};

#endif /* _PACKET_H_ */