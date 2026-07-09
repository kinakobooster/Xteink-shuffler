#pragma once

#include <GxEPD2_BW.h>
#include <SD.h>

using ShufflerDisplay = GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT>;

// preferPartial: true = use partial refresh when possible (faster card draws).
// cacheCover: store buffer for instant cover redraw (Back button).
bool drawBitmapFromSD(ShufflerDisplay &display,
                      const char *path,
                      bool preferPartial = false,
                      bool cacheCover = false,
                      bool flipVertical = false);

// Draw three card BMPs in triangle layout.
// flipVertical[i] = true draws that card upside down (逆位置).
bool drawThreeBitmapsFromSD(ShufflerDisplay &display,
                            const char *paths[3],
                            bool preferPartial = false,
                            const bool flipVertical[3] = nullptr);

// Show cached cover if path matches; returns false if cache miss.
bool drawCachedCover(ShufflerDisplay &display, const char *path);

// Clear cover cache (call on deck change).
void invalidateCoverCache();
