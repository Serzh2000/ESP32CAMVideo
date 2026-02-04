#include "AviUtils.h"

// Функция для записи 4-байтового числа в файл (Little Endian)
// Little Endian - порядок байтов, где младший байт идет первым.
void print_quartet(unsigned long i, File &fd) {
  fd.write(i % 256); i = i >> 8;
  fd.write(i % 256); i = i >> 8;
  fd.write(i % 256); i = i >> 8;
  fd.write(i % 256);
}

// Запись и обновление заголовка AVI файла
// Вызывается в конце записи для простановки реальных размеров файла и длительности
void write_avi_header(File &fd, uint8_t* buf, int frames, int width, int height, int fps, int frames_size) {
  fd.write(buf, avi_header_size);
  
  unsigned long file_size = fd.size();
  unsigned long riff_size = file_size - 8;
  unsigned long movi_size = frames_size + 4; 
  
  // Обновляем размер RIFF (весь файл минус 8 байт заголовка)
  fd.seek(4); print_quartet(riff_size, fd);
  
  // Обновляем параметры ширины, высоты и FPS в основном заголовке
  unsigned long usec_per_frame = 1000000 / fps; // Микросекунд на кадр
  fd.seek(0x20); print_quartet(usec_per_frame, fd);
  fd.seek(0x84); print_quartet(fps, fd);
  
  fd.seek(0x40); print_quartet(width, fd);
  fd.seek(0x44); print_quartet(height, fd);
  
  fd.seek(0x30); print_quartet(frames, fd); // Общее количество кадров
  
  // Обновляем заголовок потока (stream header)
  fd.seek(0x8C); print_quartet(frames, fd); // Длина потока
  fd.seek(0xA8); print_quartet(width, fd);
  fd.seek(0xAC); print_quartet(height, fd);
  
  // Обновляем размер блока movi (где лежат данные видео)
  fd.seek(0xE0); print_quartet(movi_size, fd); 
}

// Подготовка буфера с шаблоном AVI заголовка
// Заполняет структуру стандартными значениями для MJPEG видео
void prepare_avi_header_buffer(uint8_t* buf, int width, int height, int fps) {
  // Обнуляем буфер
  memset(buf, 0, avi_header_size);
  
  // RIFF заголовок
  memcpy(buf, "RIFF", 4); 
  // [4-7] Размер файла (будет заполнен позже)
  memcpy(buf + 8, "AVI ", 4);
  
  // LIST hdrl (Заголовок списка заголовков)
  memcpy(buf + 12, "LIST", 4);
  // [16-19] Размер списка (208 байт)
  buf[16] = 208; buf[17] = 0; buf[18] = 0; buf[19] = 0;
  memcpy(buf + 20, "hdrl", 4);
  
  // avih (Главный AVI заголовок)
  memcpy(buf + 24, "avih", 4);
  // [28-31] Размер заголовка (56 байт)
  buf[28] = 56; buf[29] = 0; buf[30] = 0; buf[31] = 0;
  // [32-35] Микросекунд на кадр
  // [36-39] Макс. байт в секунду
  // [40-43] Выравнивание
  // [44-47] Флаги
  // [48-51] Всего кадров
  // [52-55] Начальный кадр
  // [56-59] Количество потоков (1)
  buf[56] = 1;
  // [60-63] Размер буфера
  // [64-67] Ширина
  // [68-71] Высота
  // ... остальные поля
  
  // LIST strl (Заголовок списка потоков)
  memcpy(buf + 88, "LIST", 4);
  // [92-95] Размер (116 байт)
  buf[92] = 116; buf[93] = 0; buf[94] = 0; buf[95] = 0;
  memcpy(buf + 96, "strl", 4);
  
  // strh (Заголовок потока)
  memcpy(buf + 100, "strh", 4);
  // [104-107] Размер (56 байт)
  buf[104] = 56; buf[105] = 0; buf[106] = 0; buf[107] = 0;
  memcpy(buf + 108, "vids", 4); // Тип: Видео
  memcpy(buf + 112, "MJPG", 4); // Кодек: MJPEG
  // ...
  buf[128] = 1; // Scale = 1
  // ...
  // [144-145] Размер буфера (32768 байт)
  buf[144] = 0x00; buf[145] = 0x80; 
  // [148-151] Качество (-1 = default)
  buf[148] = 0xFF; buf[149] = 0xFF; buf[150] = 0xFF; buf[151] = 0xFF;
  
  // strf (Формат потока)
  memcpy(buf + 164, "strf", 4);
  // [168-171] Размер (40 байт)
  buf[168] = 40; buf[169] = 0; buf[170] = 0; buf[171] = 0;
  // biSize (40)
  buf[172] = 40;
  // [176] Ширина
  // [180] Высота
  // [184] Плоскости (1)
  buf[184] = 1;
  // [186] Бит на пиксель (24)
  buf[186] = 24;
  // [188] Сжатие (MJPG)
  memcpy(buf + 188, "MJPG", 4);
  
  // LIST movi (Список данных видео)
  memcpy(buf + 212, "LIST", 4);
  // [216] Размер списка (будет заполнен позже)
  memcpy(buf + 220, "movi", 4);
  
  // Далее следуют данные кадров...
}
