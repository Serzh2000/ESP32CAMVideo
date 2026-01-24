#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <Arduino.h>
#include "esp_camera.h"

String recordVideo(int recordDuration, int fps, String ssid, String password);

#endif
