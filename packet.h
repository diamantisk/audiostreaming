#define BUFSIZE 8820
#define PORT 12345
#define FILESIZE_MAX 100

struct __attribute__((packed)) audio_info {
    int channels;
    int sample_size;
    int sample_rate;
    long time_per_packet;   // Nanoseconds
    char filename[FILESIZE_MAX];
    enum flag {SUCCESS, FILE_NOT_FOUND, FAILURE} status;
};

struct __attribute__((packed)) packet {
    int seq;
    char buffer[BUFSIZE];
};