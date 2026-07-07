#include "BmpSdDraw.h"

#include <SPI.h>
#include <string.h>

namespace
{
constexpr uint16_t PHYS_WIDTH = 800;
constexpr uint16_t PHYS_HEIGHT = 480;
constexpr uint16_t PHYS_ROW_BYTES = PHYS_WIDTH / 8;

constexpr uint16_t INPUT_BUFFER_PIXELS = 20;
constexpr uint16_t MAX_ROW_WIDTH = 800;
constexpr uint16_t MAX_PALETTE_PIXELS = 256;

uint8_t inputBuffer[3 * INPUT_BUFFER_PIXELS];
uint8_t monoPalette[MAX_PALETTE_PIXELS / 8];
uint8_t colorPalette[MAX_PALETTE_PIXELS / 8];
uint8_t physicalRows[PHYS_HEIGHT][PHYS_ROW_BYTES];
uint8_t colorRow[PHYS_ROW_BYTES];

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

void mapAndSetPixel(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT> &display,
                    int16_t gx,
                    int16_t gy,
                    bool black)
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
} // namespace

bool drawBitmapFromSD(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT> &display,
                      const char *path,
                      int16_t x,
                      int16_t y)
{
  (void)x;
  (void)y;

  File file = SD.open(path, FILE_READ);
  if (!file)
  {
    Serial.printf("BMP not found: %s\n", path);
    return false;
  }

  bool valid = false;
  bool flip = true;
  bool withColor = true;

  if (read16(file) != 0x4D42)
  {
    file.close();
    Serial.printf("Not a BMP: %s\n", path);
    return false;
  }

  const uint32_t fileSize = read32(file);
  (void)fileSize;
  (void)read32(file);
  const uint32_t imageOffset = read32(file);
  const uint32_t headerSize = read32(file);
  (void)headerSize;
  const uint32_t width = read32(file);
  int32_t height = static_cast<int32_t>(read32(file));
  const uint16_t planes = read16(file);
  const uint16_t depth = read16(file);
  const uint32_t format = read32(file);

  if (planes != 1 || (format != 0 && format != 3))
  {
    file.close();
    Serial.printf("Unsupported BMP format: %s\n", path);
    return false;
  }

  uint32_t rowSize = (width * depth / 8 + 3) & ~3;
  if (depth < 8)
  {
    rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
  }

  if (height < 0)
  {
    height = -height;
    flip = false;
  }

  const int16_t expectedW = display.width();
  const int16_t expectedH = display.height();
  if (int32_t(width) != expectedW || height != expectedH)
  {
    Serial.printf("BMP size mismatch %s: %ldx%ld (expected %dx%d)\n",
                  path, static_cast<long>(width), static_cast<long>(height),
                  expectedW, expectedH);
    file.close();
    return false;
  }

  clearPhysicalRows();

  uint8_t bitmask = 0xFF;
  const uint8_t bitshift = 8 - depth;
  uint16_t red = 0;
  uint16_t green = 0;
  uint16_t blue = 0;
  bool whitish = false;
  bool colored = false;

  if (depth == 1)
  {
    withColor = false;
  }

  if (depth <= 8)
  {
    if (depth < 8)
    {
      bitmask >>= depth;
    }

    file.seek(imageOffset - (4 << depth));
    for (int16_t pn = 0; pn < (1 << depth); pn++)
    {
      blue = file.read();
      green = file.read();
      red = file.read();
      file.read();
      whitish = withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                          : ((red + green + blue) > 3 * 0x80);
      colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
      if (0 == pn % 8)
      {
        monoPalette[pn / 8] = 0;
        colorPalette[pn / 8] = 0;
      }
      monoPalette[pn / 8] |= whitish << (pn % 8);
      colorPalette[pn / 8] |= colored << (pn % 8);
    }
  }

  const uint16_t w = static_cast<uint16_t>(width);
  const uint16_t h = static_cast<uint16_t>(height);

  uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
  for (uint16_t row = 0; row < h; row++, rowPosition += rowSize)
  {
    const int16_t gy = flip ? static_cast<int16_t>(h - row - 1) : static_cast<int16_t>(row);

    uint32_t inRemain = rowSize;
    uint32_t inIdx = 0;
    uint32_t inBytes = 0;
    uint8_t inByte = 0;
    uint8_t inBits = 0;

    file.seek(rowPosition);
    for (uint16_t col = 0; col < w; col++)
    {
      if (inIdx >= inBytes)
      {
        inBytes = file.read(inputBuffer, inRemain > sizeof(inputBuffer) ? sizeof(inputBuffer) : inRemain);
        inRemain -= inBytes;
        inIdx = 0;
      }

      switch (depth)
      {
      case 32:
        blue = inputBuffer[inIdx++];
        green = inputBuffer[inIdx++];
        red = inputBuffer[inIdx++];
        inIdx++;
        whitish = withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                            : ((red + green + blue) > 3 * 0x80);
        colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
        break;
      case 24:
        blue = inputBuffer[inIdx++];
        green = inputBuffer[inIdx++];
        red = inputBuffer[inIdx++];
        whitish = withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                            : ((red + green + blue) > 3 * 0x80);
        colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
        break;
      case 16:
      {
        const uint8_t lsb = inputBuffer[inIdx++];
        const uint8_t msb = inputBuffer[inIdx++];
        if (format == 0)
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
        whitish = withColor ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
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
        const uint16_t pn = (inByte >> bitshift) & bitmask;
        whitish = monoPalette[pn / 8] & (0x1 << (pn % 8));
        colored = colorPalette[pn / 8] & (0x1 << (pn % 8));
        inByte <<= depth;
        inBits -= depth;
      }
      break;
      default:
        whitish = true;
        colored = false;
        break;
      }

      const bool black = !whitish && !(colored && withColor);
      mapAndSetPixel(display, static_cast<int16_t>(col), gy, black);
    }
  }

  file.close();
  valid = true;

  Serial.printf("BMP mapped: %s (%ux%u)\n", path, w, h);
  display.writeScreenBuffer();
  for (uint16_t py = 0; py < PHYS_HEIGHT; py++)
  {
    display.writeImage(physicalRows[py], colorRow, 0, static_cast<int16_t>(py), PHYS_WIDTH, 1);
  }
  display.refresh(false);

  return valid;
}
