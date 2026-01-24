#ifndef AVI_UTILS_H
#define AVI_UTILS_H

#include <Arduino.h>
#include <FS.h>

const int avi_header_size = 252;

struct AviIndexEntry {
  uint32_t offset;
  uint32_t size;
};

void print_quartet(unsigned long i, File &fd);
void write_avi_header(File &fd, uint8_t* buf, int frames, int width, int height, int fps, int frames_size);
void prepare_avi_header_buffer(uint8_t* buf, int width, int height, int fps);

#endif
