#include "packet.h"

//int BUFSIZE = 5000;
//int PORT = 12345;
//int FILESIZE_MAX = 100;
//
//struct __attribute__((packed)) audio_info {
//    int channels;
//    int sample_size;
//    int sample_rate;
//    long time_per_packet;   // Nanoseconds
//    char filename[FILESIZE_MAX];
//    enum flag {SUCCESS, FILE_NOT_FOUND, FAILURE} status;
//};
//
//struct __attribute__((packed)) audio_packet {
//    int seq;
//    int audiobytesread;
//    char buffer[BUFSIZE];
//};