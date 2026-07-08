#pragma once

#include <GxEPD2_BW.h>
#include <SD.h>

using ShufflerDisplay = GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT>;

// Draw a full-screen card BMP from SD.
bool drawBitmapFromSD(ShufflerDisplay &display, const char *path, int16_t x = 0, int16_t y = 0);

// Draw three card BMPs side-by-side (uses full screen width).
bool drawThreeBitmapsFromSD(ShufflerDisplay &display, const char *paths[3]);
