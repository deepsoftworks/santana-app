//
// stresstest santana
//
// License: Apache 2.0
//

// This is needed for musl libc
#if ! defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 500)
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

const char help[] =
    "Usage:\n"
    "  stresstest [-c] [-g] [-n] [-r rate] [-s seed]\n"
    "  stresstest -h\n"
    "\n"
    "  -h       print this help message and exit\n"
    "  -c       randomly chunk the output\n"
    "  -g       occasionally output garbage (non-numeric lines)\n"
    "  -n       output negative values (wave centered at 0)\n"
    "  -r rate  sample rate in samples/s (default: 100)\n"
    "  -s seed  set random seed\n"
    "\n"
    "Pipe into santana, e.g.:\n"
    "  ./stresstest | ./build/santana -t 'Stress Test' -u 'val'\n"
    "  ./stresstest -g -c | ./build/santana -t 'Chunked + Garbage'\n";

const char optstring[] = "hcgnr:s:";

int main(int argc, char *argv[]) {
    char buffer[1024];
    size_t buffer_pos = 0;
    int opt;
    bool chunked        = false;
    bool add_garbage    = false;
    bool output_negative = false;
    double rate = 100.0;
    unsigned int seed = (unsigned int)time(NULL);

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'h':
                printf("%s", help);
                return EXIT_SUCCESS;
            case 'c':
                chunked = true;
                break;
            case 'g':
                add_garbage = true;
                break;
            case 'n':
                output_negative = true;
                break;
            case 'r':
                rate = atof(optarg);
                if (rate <= 0) { fprintf(stderr, "rate must be > 0\n"); return EXIT_FAILURE; }
                break;
            case 's':
                seed = (unsigned int)atoi(optarg);
                break;
            default:
                fprintf(stderr, "%s", help);
                return EXIT_FAILURE;
        }
    }
    if (argc > optind) {
        fprintf(stderr, "%s", help);
        return EXIT_FAILURE;
    }

    const useconds_t delay = (useconds_t)(1e6 / rate);
    srand(seed);

    for (unsigned int n = 0;; n += 5) {
        double val = sin(n * M_PI / 180.0) * 5.0 + (output_negative ? 0.0 : 5.0);
        buffer_pos += (size_t)sprintf(buffer + buffer_pos, "%.4f\n", val);

        // Occasionally inject a non-numeric garbage line — santana should skip it
        if (add_garbage && rand() <= RAND_MAX / 5)
            buffer_pos += (size_t)sprintf(buffer + buffer_pos, "garbage\n");

        if (chunked) {
            // Drip bytes out in small random chunks, like a slow pipe
            size_t send_pos = 0;
            while (buffer_pos - send_pos >= 16) {
                size_t bytes_to_send = 1 + (size_t)(rand() % 16);  // 1..16
                ssize_t bytes_sent =
                    write(STDOUT_FILENO, buffer + send_pos, bytes_to_send);
                usleep(50);
                if (bytes_sent > 0)
                    send_pos += (size_t)bytes_sent;
            }
            if (send_pos > 0 && send_pos < buffer_pos)
                memmove(buffer, buffer + send_pos, buffer_pos - send_pos);
            buffer_pos -= send_pos;
        } else {
            ssize_t bytes_sent = write(STDOUT_FILENO, buffer, buffer_pos);
            if (bytes_sent > 0 && (size_t)bytes_sent < buffer_pos)
                memmove(buffer, buffer + bytes_sent, buffer_pos - bytes_sent);
            if (bytes_sent > 0)
                buffer_pos -= (size_t)bytes_sent;
        }

        usleep(delay);
    }

    return 0;
}
