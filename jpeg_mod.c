#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jpeg_mod.h"

// Function to load all filenames from a directory
int load_filenames(const char *directory, unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN]) {
    struct dirent **namelist;
    int n, count = 0;
    char path[MAX_FILENAME_LEN];
    struct stat file_stat;

    // Use scandir to read all entries in the directory
    n = scandir(directory, &namelist, NULL, alphasort);
    if (n < 0) {
        perror("scandir");
        return -1;
    }

    // Iterate over the directory entries
    for (int i = 0; i < n && count < MAX_FILES; i++) {
        // Construct the full path
        snprintf(path, sizeof(path), "%s/%s", directory, namelist[i]->d_name);

        // Use stat to check if it is a directory
        if (stat(path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
            free(namelist[i]); // Skip directories
            continue;
        }

        // Copy the full path into the filenames array
        strncpy(filenames[count], path, MAX_FILENAME_LEN - 1);
        filenames[count][MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
        count++;

        free(namelist[i]); // Free the dirent struct allocated by scandir
    }
    free(namelist); // Free the array of pointers allocated by scandir

    return count;
}

int initializeJpegImage(JpegImage* image, const char* filename) 
{
    // Initialize JPEG decompression object and error handling
    struct jpeg_error_mgr* jerr = malloc(sizeof(struct jpeg_error_mgr));
    if (!jerr) {
        fprintf(stderr, "Error: Could not allocate memory for jpeg_error_mgr\n");
        return 1;
    }
    image->cinfo.err = jpeg_std_error(jerr);
    jpeg_create_decompress(&image->cinfo);

    // Open the JPEG file
    image->file = fopen(filename, "rb");
    if (!image->file) {
        fprintf(stderr, "Error: Could not open %s\n", filename);
        jpeg_destroy_decompress(&image->cinfo);
        free(jerr);
        return 1;
    }

    // Set file source and read JPEG header
    jpeg_stdio_src(&image->cinfo, image->file);
    if (jpeg_read_header(&image->cinfo, TRUE) != JPEG_HEADER_OK) {
        fprintf(stderr, "Error: Could not read JPEG header\n");
        fclose(image->file);
        jpeg_destroy_decompress(&image->cinfo);
        free(jerr);
        return 1;
    }

    // Start decompression
    jpeg_start_decompress(&image->cinfo);

    // Get image dimensions
    image->width = image->cinfo.output_width;
    image->height = image->cinfo.output_height;

    // Allocate buffer for RGB data
    int row_stride = image->width * image->cinfo.output_components;
    image->rgb_buffer = (unsigned char *)malloc(row_stride * image->height);
    if (!image->rgb_buffer) {
        fprintf(stderr, "Error: Could not allocate memory for RGB buffer\n");
        jpeg_finish_decompress(&image->cinfo);
        jpeg_destroy_decompress(&image->cinfo);
        free(jerr);
        return 1;
    }

    // Read each scanline into RGB buffer
    while (image->cinfo.output_scanline < image->cinfo.output_height) {
        unsigned char *row_pointer = image->rgb_buffer + (image->cinfo.output_scanline * row_stride);
        jpeg_read_scanlines(&image->cinfo, &row_pointer, 1);
    }

    return 0;  // Success
}

void cleanupJpegImage(JpegImage* image) {
    if (image->file) {
        fclose(image->file);
    }

    if (image->rgb_buffer) {
        free(image->rgb_buffer);
    }

    if (image->yuv_buffer) {
        free(image->yuv_buffer);
    }

    jpeg_finish_decompress(&image->cinfo);
    jpeg_destroy_decompress(&image->cinfo);
    free(image->cinfo.err);  // Free error manager
}

int displayImg(JpegImage* image) 
{
    // Set up SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create SDL window and renderer
    SDL_Window *window = SDL_CreateWindow("JPEG Image", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          image->cinfo.output_width, image->cinfo.output_height, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create SDL texture for the image
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             image->cinfo.output_width, image->cinfo.output_height);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Allocate buffer to store the image data
    unsigned char *image_data = (unsigned char *)malloc(image->cinfo.output_width * image->cinfo.output_height * image->cinfo.output_components);

    // Read scanlines and fill the buffer with raw RGB data
    while (image->cinfo.output_scanline < image->cinfo.output_height) {
        unsigned char *row_pointer = image_data + image->cinfo.output_scanline * image->cinfo.output_width * image->cinfo.output_components;
        jpeg_read_scanlines(&image->cinfo, &row_pointer, 1);
    }

    // Update the texture with the image data
    SDL_UpdateTexture(texture, NULL, image_data, image->cinfo.output_width * image->cinfo.output_components);

    // Display the image
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    // Wait for a quit event
    SDL_Event event;
    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            }
        }
    }

    // Cleanup
    free(image_data);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
                                 
// Function to convert RGB to YUV format
int convertRGBtoYUV(JpegImage* image) {
    int width = image->width;
    int height = image->height;
    int frame_size = width * height;
    image->yuv_buffer = (unsigned char *)malloc(frame_size * 3 / 2);  // For YUV 4:2:0

    unsigned char *Y = image->yuv_buffer;
    unsigned char *U = Y + frame_size;
    unsigned char *V = U + frame_size / 4;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 3;
            unsigned char r = image->rgb_buffer[index];
            unsigned char g = image->rgb_buffer[index + 1];
            unsigned char b = image->rgb_buffer[index + 2];

            // Convert RGB to YUV
            int Y_index = y * width + x;
            Y[Y_index] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);

            if (y % 2 == 0 && x % 2 == 0) { // Subsample U and V for 4:2:0
                int uv_index = (y / 2) * (width / 2) + (x / 2);
                U[uv_index] = (unsigned char)(-0.147 * r - 0.289 * g + 0.436 * b + 128);
                V[uv_index] = (unsigned char)(0.615 * r - 0.515 * g - 0.100 * b + 128);
            }
        }
    }

    return 0;
}

unsigned char* getYUVBuffer(JpegImage *image, size_t *length) 
{
    int y_size = image->width * image->height;
    int uv_size = y_size / 4;
    *length = y_size + 2 * uv_size;  // Total size of YUV 4:2:0 data
    return image->yuv_buffer;
}