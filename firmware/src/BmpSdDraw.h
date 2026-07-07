#pragma once

#include <GxEPD2_BW.h>
#include <SD.h>

// Draw a 1-bit (or compatible) BMP from the SD card onto the e-paper display.
// Returns true when the bitmap was rendered.
bool drawBitmapFromSD(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT> &display,
                      const char *path,
                      int16_t x = 0,
                      int16_t y = 0);
