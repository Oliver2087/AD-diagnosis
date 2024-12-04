#ifndef JPEG_MOD_H_
#define JPEG_MOD_H_

#include <jpeglib.h>
#include <SDL2/SDL.h>

#define MAX_FILES 100  // Adjust this based on the number of files you expect
#define MAX_FILENAME_LEN 256 // Maximum filename length
#define JPEG_DIRECTORY "examples/Video2/Images"

typedef struct 
{
    struct jpeg_decompress_struct cinfo;
    unsigned char *rgb_buffer;
    unsigned char *yuv_buffer;
    int width;
    int height;
    FILE *file;
} JpegImage;

int load_filenames(const char *directory, unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN]);
int initializeJpegImage(JpegImage* image, const char* filename);
void cleanupJpegImage(JpegImage* image);
int readSingleJpeg(const char* filename, JpegImage* image);
int displayImg(JpegImage* image);
int convertRGBtoYUV(JpegImage* image);

#endif