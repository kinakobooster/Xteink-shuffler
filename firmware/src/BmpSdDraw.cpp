#include "BmpSdDraw.h"

#include <SPI.h>
#include <string.h>

namespace
{
constexpr uint16_t PHYS_WIDTH = 800;
constexpr uint16_t PHYS_HEIGHT = 480;
constexpr uint16_t PHYS_ROW_BYTES = PHYS_WIDTH / 8;
constexpr uint16_t INPUT_BUFFER_PIXELS = 20;
constexpr uint16_t MAX_PALETTE_PIXELS = 256;
constexpr uint16_t MAX_BMP_WIDTH = 480;

uint8_t inputBuffer[3 * INPUT_BUFFER_PIXELS];
uint8_t monoPalette[MAX_PALETTE_PIXELS / 8];
uint8_t colorPalette[MAX_PALETTE_PIXELS / 8];
uint8_t physicalRows[PHYS_HEIGHT][PHYS_ROW_BYTES];
uint8_t colorRow[PHYS_ROW_BYTES];
bool rowBlack[MAX_BMP_WIDTH];

struct Rect
{
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

void computeAspectFitSize(int16_t srcW, int16_t srcH, int16_t maxW, int16_t maxH, int16_t &outW, int16_t &outH)
{
  if (srcW <= 0 || srcH <= 0 || maxW <= 0 || maxH <= 0)
  {
    outW = 0;
    outH = 0;
    return;
  }

  if (static_cast<int32_t>(maxW) * srcH <= static_cast<int32_t>(maxH) * srcW)
  {
    outW = maxW;
    outH = static_cast<int16_t>((static_cast<int32_t>(maxW) * srcH) / srcW);
  }
  else
  {
    outH = maxH;
    outW = static_cast<int16_t>((static_cast<int32_t>(maxH) * srcW) / srcH);
  }
}

void computeTriangleLayout(int16_t screenW, int16_t screenH, int16_t srcW, int16_t srcH, Rect out[3])
{
  constexpr int16_t MARGIN = 8;
  constexpr int16_t GAP = 10;
  constexpr int16_t ROW_GAP = 14;

  const int16_t bottomMaxW = (screenW - 2 * MARGIN - GAP) / 2;
  const int16_t totalMaxH = screenH - 2 * MARGIN - ROW_GAP;
  const int16_t rowMaxH = totalMaxH / 2;

  int16_t cardW = 0;
  int16_t cardH = 0;
  computeAspectFitSize(srcW, srcH, bottomMaxW, rowMaxH, cardW, cardH);

  const int16_t topX = (screenW - cardW) / 2;
  const int16_t topY = MARGIN;
  const int16_t bottomY = screenH - MARGIN - cardH;

  out[0] = {topX, topY, cardW, cardH};
  out[1] = {MARGIN, bottomY, cardW, cardH};
  out[2] = {static_cast<int16_t>(MARGIN + cardW + GAP), bottomY, cardW, cardH};
}

struct BmpInfo
{
  uint32_t imageOffset = 0;
  uint32_t rowSize = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t depth = 0;
  uint32_t format = 0;
  bool flip = true;
  bool withColor = true;
  uint8_t bitmask = 0xFF;
  uint8_t bitshift = 0;
};

uint16_t read16(File &file)
{
  uint16_t result;
  ((uint8_t *)&result)[0] = file.read();
  ((uint8_t *)&result)[1] = file.read();
  return result;
}

uint32_t read32(File &file)
{
  uint32_t result;
  ((uint8_t *)&result)[0] = file.read();
  ((uint8_t *)&result)[1] = file.read();
  ((uint8_t *)&result)[2] = file.read();
  ((uint8_t *)&result)[3] = file.read();
  return result;
}

void clearPhysicalRows()
{
  for (uint16_t y = 0; y < PHYS_HEIGHT; y++)
  {
    memset(physicalRows[y], 0xFF, PHYS_ROW_BYTES);
  }
  memset(colorRow, 0xFF, sizeof(colorRow));
}

void setPhysicalPixel(int16_t px, int16_t py, bool black)
{
  if (px < 0 || px >= int16_t(PHYS_WIDTH) || py < 0 || py >= int16_t(PHYS_HEIGHT))
  {
    return;
  }

  if (black)
  {
    physicalRows[py][px / 8] &= static_cast<uint8_t>(~(0x80 >> (px % 8)));
  }
}

void mapAndSetPixel(ShufflerDisplay &display, int16_t gx, int16_t gy, bool black)
{
  int16_t px = gx;
  int16_t py = gy;

  switch (display.getRotation())
  {
  case 1:
    px = gy;
    py = GxEPD2_426_GDEQ0426T82::HEIGHT - gx - 1;
    break;
  case 2:
    px = GxEPD2_426_GDEQ0426T82::WIDTH - gx - 1;
    py = GxEPD2_426_GDEQ0426T82::HEIGHT - gy - 1;
    break;
  case 3:
    px = gy;
    py = GxEPD2_426_GDEQ0426T82::HEIGHT - gx - 1;
    break;
  default:
    break;
  }

  setPhysicalPixel(px, py, black);
}

void flushPhysicalDisplay(ShufflerDisplay &display)
{
  display.writeScreenBuffer();
  for (uint16_t py = 0; py < PHYS_HEIGHT; py++)
  {
    display.writeImage(physicalRows[py], colorRow, 0, static_cast<int16_t>(py), PHYS_WIDTH, 1);
  }
  display.refresh(false);
}

bool loadPalette(File &file, BmpInfo &info)
{
  if (info.depth == 1)
  {
    info.withColor = false;
  }

  if (info.depth > 8)
  {
    return true;
  }

  if (info.depth < 8)
  {
    info.bitmask >>= info.depth;
  }

  file.seek(info.imageOffset - (4 << info.depth));
  for (int16_t pn = 0; pn < (1 << info.depth); pn++)
  {
    const uint16_t blue = file.read();
    const uint16_t green = file.read();
    const uint16_t red = file.read();
    file.read();
    const bool whitish = info.withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                        : ((red + green + blue) > 3 * 0x80);
    const bool colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
    if (0 == pn % 8)
    {
      monoPalette[pn / 8] = 0;
      colorPalette[pn / 8] = 0;
    }
    monoPalette[pn / 8] |= whitish << (pn % 8);
    colorPalette[pn / 8] |= colored << (pn % 8);
  }

  return true;
}

bool parseBmpHeader(File &file, BmpInfo &info)
{
  if (read16(file) != 0x4D42)
  {
    return false;
  }

  (void)read32(file);
  (void)read32(file);
  info.imageOffset = read32(file);
  (void)read32(file);
  info.width = static_cast<uint16_t>(read32(file));
  int32_t height = static_cast<int32_t>(read32(file));
  const uint16_t planes = read16(file);
  info.depth = read16(file);
  info.format = read32(file);

  if (planes != 1 || (info.format != 0 && info.format != 3))
  {
    return false;
  }

  info.rowSize = (info.width * info.depth / 8 + 3) & ~3;
  if (info.depth < 8)
  {
    info.rowSize = ((info.width * info.depth + 8 - info.depth) / 8 + 3) & ~3;
  }

  info.flip = true;
  if (height < 0)
  {
    height = -height;
    info.flip = false;
  }
  info.height = static_cast<uint16_t>(height);
  info.bitmask = 0xFF;
  info.bitshift = 8 - info.depth;

  return loadPalette(file, info);
}

bool readBmpRowBits(File &file, const BmpInfo &info, uint16_t srcRow, bool *outBits)
{
  if (srcRow >= info.height || info.width > MAX_BMP_WIDTH)
  {
    return false;
  }

  const uint32_t rowPosition = info.flip
                                   ? info.imageOffset + (info.height - srcRow - 1) * info.rowSize
                                   : info.imageOffset + srcRow * info.rowSize;

  uint32_t inRemain = info.rowSize;
  uint32_t inIdx = 0;
  uint32_t inBytes = 0;
  uint8_t inByte = 0;
  uint8_t inBits = 0;

  file.seek(rowPosition);
  for (uint16_t col = 0; col < info.width; col++)
  {
    if (inIdx >= inBytes)
    {
      inBytes = file.read(inputBuffer, inRemain > sizeof(inputBuffer) ? sizeof(inputBuffer) : inRemain);
      inRemain -= inBytes;
      inIdx = 0;
    }

    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;
    bool whitish = true;
    bool colored = false;

    switch (info.depth)
    {
    case 32:
      blue = inputBuffer[inIdx++];
      green = inputBuffer[inIdx++];
      red = inputBuffer[inIdx++];
      inIdx++;
      whitish = info.withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                               : ((red + green + blue) > 3 * 0x80);
      colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
      break;
    case 24:
      blue = inputBuffer[inIdx++];
      green = inputBuffer[inIdx++];
      red = inputBuffer[inIdx++];
      whitish = info.withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                               : ((red + green + blue) > 3 * 0x80);
      colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
      break;
    case 16:
    {
      const uint8_t lsb = inputBuffer[inIdx++];
      const uint8_t msb = inputBuffer[inIdx++];
      if (info.format == 0)
      {
        blue = (lsb & 0x1F) << 3;
        green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
        red = (msb & 0x7C) << 1;
      }
      else
      {
        blue = (lsb & 0x1F) << 3;
        green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
        red = (msb & 0xF8);
      }
      whitish = info.withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                               : ((red + green + blue) > 3 * 0x80);
      colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
    }
    break;
    case 1:
    case 2:
    case 4:
    case 8:
    {
      if (0 == inBits)
      {
        inByte = inputBuffer[inIdx++];
        inBits = 8;
      }
      const uint16_t pn = (inByte >> info.bitshift) & info.bitmask;
      whitish = monoPalette[pn / 8] & (0x1 << (pn % 8));
      colored = colorPalette[pn / 8] & (0x1 << (pn % 8));
      inByte <<= info.depth;
      inBits -= info.depth;
    }
    break;
    default:
      whitish = true;
      colored = false;
      break;
    }

    outBits[col] = !whitish && !(colored && info.withColor);
  }

  return true;
}

bool drawBitmapScaledToSlot(ShufflerDisplay &display,
                            File &file,
                            const BmpInfo &info,
                            int16_t slotGx,
                            int16_t slotGy,
                            int16_t slotW,
                            int16_t slotH)
{
  for (int16_t dy = 0; dy < slotH; dy++)
  {
    const uint16_t srcY = static_cast<uint16_t>((static_cast<int32_t>(dy) * info.height) / slotH);
    if (!readBmpRowBits(file, info, srcY, rowBlack))
    {
      return false;
    }

    for (int16_t dx = 0; dx < slotW; dx++)
    {
      const uint16_t srcX = static_cast<uint16_t>((static_cast<int32_t>(dx) * info.width) / slotW);
      mapAndSetPixel(display, slotGx + dx, slotGy + dy, rowBlack[srcX]);
    }
  }

  return true;
}

bool drawBitmapScaledFromSD(ShufflerDisplay &display,
                            const char *path,
                            int16_t slotGx,
                            int16_t slotGy,
                            int16_t slotW,
                            int16_t slotH,
                            int16_t expectedW,
                            int16_t expectedH)
{
  File file = SD.open(path, FILE_READ);
  if (!file)
  {
    Serial.printf("BMP not found: %s\n", path);
    return false;
  }

  BmpInfo info;
  if (!parseBmpHeader(file, info))
  {
    file.close();
    Serial.printf("Unsupported BMP: %s\n", path);
    return false;
  }

  if (int16_t(info.width) != expectedW || int16_t(info.height) != expectedH)
  {
    Serial.printf("BMP size mismatch %s: %ux%u\n", path, info.width, info.height);
    file.close();
    return false;
  }

  const bool ok = drawBitmapScaledToSlot(display, file, info, slotGx, slotGy, slotW, slotH);
  file.close();
  return ok;
}
} // namespace

bool drawBitmapFromSD(ShufflerDisplay &display, const char *path, int16_t x, int16_t y)
{
  (void)x;
  (void)y;

  clearPhysicalRows();

  const int16_t screenW = display.width();
  const int16_t screenH = display.height();
  int16_t fitW = 0;
  int16_t fitH = 0;
  computeAspectFitSize(screenW, screenH, screenW, screenH, fitW, fitH);
  const int16_t fitX = (screenW - fitW) / 2;
  const int16_t fitY = (screenH - fitH) / 2;

  const bool ok = drawBitmapScaledFromSD(display, path, fitX, fitY, fitW, fitH, screenW, screenH);
  if (!ok)
  {
    return false;
  }

  Serial.printf("BMP full: %s\n", path);
  flushPhysicalDisplay(display);
  return true;
}

bool drawThreeBitmapsFromSD(ShufflerDisplay &display, const char *paths[3])
{
  const int16_t screenW = display.width();
  const int16_t screenH = display.height();
  Rect slots[3];
  computeTriangleLayout(screenW, screenH, screenW, screenH, slots);

  clearPhysicalRows();

  for (int i = 0; i < 3; i++)
  {
    if (!drawBitmapScaledFromSD(display, paths[i], slots[i].x, slots[i].y, slots[i].w, slots[i].h,
                                screenW, screenH))
    {
      Serial.printf("Triple draw failed at slot %d: %s\n", i, paths[i]);
      return false;
    }
  }

  Serial.printf("BMP triple (triangle): %s | %s | %s\n", paths[0], paths[1], paths[2]);
  flushPhysicalDisplay(display);
  return true;
}
