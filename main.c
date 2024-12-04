#include <stdio.h>
#include <stdlib.h>
#include "encoder.h"

const char *output_filename = "test_Vid.mpeg";
const char *directory = "/home/wsoxl/video_encoder/include/examples/Video2/Images";

int main() {
    unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN];
    int count;

    // Load filenames from the specified directory
    count = load_filenames(directory, filenames);


    if (count >= 0) {
        // Print how many files are found
        printf("Found %d files:\n", count);

        // Print read files
        for (int i = 0; i < count; i++) {
            //printf("%s\n", filenames[i]);
        }
    }

    int width = 3840;
    int height = 2160;
    int bitrate = 4000000;

    JpegImage image;

    encode_frames_to_mpeg(output_filename, filenames, width, height, 10, count, bitrate);

    return 0;
}
