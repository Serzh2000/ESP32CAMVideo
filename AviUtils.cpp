#include "AviUtils.h"

void print_quartet(unsigned long i, File &fd) {
  fd.write(i % 256); i = i >> 8;
  fd.write(i % 256); i = i >> 8;
  fd.write(i % 256); i = i >> 8;
  fd.write(i % 256);
}

void write_avi_header(File &fd, uint8_t* buf, int frames, int width, int height, int fps, int frames_size) {
  fd.write(buf, avi_header_size);
  
  unsigned long file_size = fd.size();
  unsigned long riff_size = file_size - 8;
  unsigned long movi_size = frames_size + 4; // Or file_size - 240 approx, but precise is better
  
  // Update RIFF size
  fd.seek(4); print_quartet(riff_size, fd);
  
  // Update width/height/fps in main header
  unsigned long usec_per_frame = 1000000 / fps;
  fd.seek(0x20); print_quartet(usec_per_frame, fd);
  fd.seek(0x84); print_quartet(fps, fd);
  
  fd.seek(0x40); print_quartet(width, fd);
  fd.seek(0x44); print_quartet(height, fd);
  
  fd.seek(0x30); print_quartet(frames, fd); // total frames
  
  // Update stream header
  fd.seek(0x8C); print_quartet(frames, fd); // stream length
  fd.seek(0xA8); print_quartet(width, fd);
  fd.seek(0xAC); print_quartet(height, fd);
  
  // Update movi size
  fd.seek(0xE0); print_quartet(movi_size, fd); // movi list size
}

void prepare_avi_header_buffer(uint8_t* buf, int width, int height, int fps) {
  // Initialize standard AVI header template
  // This is a minimal header for MJPEG
  memset(buf, 0, avi_header_size);
  
  // RIFF
  memcpy(buf, "RIFF", 4); 
  // size at 4
  memcpy(buf + 8, "AVI ", 4);
  
  // LIST hdrl
  memcpy(buf + 12, "LIST", 4);
  // size at 16 (208)
  buf[16] = 208; buf[17] = 0; buf[18] = 0; buf[19] = 0;
  memcpy(buf + 20, "hdrl", 4);
  
  // avih
  memcpy(buf + 24, "avih", 4);
  // size at 28 (56)
  buf[28] = 56; buf[29] = 0; buf[30] = 0; buf[31] = 0;
  // microsec per frame at 32
  // max bytes per sec at 36
  // padding at 40
  // flags at 44
  // total frames at 48
  // initial frames at 52
  // streams at 56 (1)
  buf[56] = 1;
  // buffer size at 60
  // width at 64
  // height at 68
  // scale at 72
  // rate at 76
  // start at 80
  // length at 84
  
  // LIST strl
  memcpy(buf + 88, "LIST", 4);
  // size at 92 (116)
  buf[92] = 116; buf[93] = 0; buf[94] = 0; buf[95] = 0;
  memcpy(buf + 96, "strl", 4);
  
  // strh
  memcpy(buf + 100, "strh", 4);
  // size at 104 (56)
  buf[104] = 56; buf[105] = 0; buf[106] = 0; buf[107] = 0;
  memcpy(buf + 108, "vids", 4); // type
  memcpy(buf + 112, "MJPG", 4); // handler
  // flags at 116
  // priority at 120
  // language at 122
  // initial frames at 124
  // scale at 128 (1)
  buf[128] = 1; // Scale = 1
  // rate at 132 (fps)
  // start at 136
  // length at 140 (frames)
  // buffer size at 144 (suggested buffer size)
  // Use a safe large value for QVGA MJPEG
  buf[144] = 0x00; buf[145] = 0x80; // 32768 bytes
  // quality at 148 (-1)
  buf[148] = 0xFF; buf[149] = 0xFF; buf[150] = 0xFF; buf[151] = 0xFF;
  // sample size at 152
  // rect at 156 (left, top, right, bottom)
  
  // strf
  memcpy(buf + 164, "strf", 4);
  // size at 168 (40)
  buf[168] = 40; buf[169] = 0; buf[170] = 0; buf[171] = 0;
  // biSize at 172 (40)
  buf[172] = 40;
  // width at 176
  // height at 180
  // planes at 184 (1)
  buf[184] = 1;
  // bit count at 186 (24)
  buf[186] = 24;
  // compression at 188 (MJPG)
  memcpy(buf + 188, "MJPG", 4);
  // size image at 192
  // x pels at 196
  // y pels at 200
  // clr used at 204
  // clr important at 208
  
  // LIST movi
  memcpy(buf + 212, "LIST", 4);
  // size at 216 (patched later)
  memcpy(buf + 220, "movi", 4);
  
  // Start of data
}
