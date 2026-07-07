#include "BmpSdDraw.h"

#include <SPI.h>

namespace
{
constexpr uint16_t INPUT_BUFFER_PIXELS = 20;
constexpr uint16_t MAX_ROW_WIDTH = 800;
constexpr uint16_t MAX_PALETTE_PIXELS = 256;

uint8_t inputBuffer[3 * INPUT_BUFFER_PIXELS];
uint8_t outputRowMono[MAX_ROW_WIDTH / 8];
uint8_t outputRowColor[MAX_ROW_WIDTH / 8];
uint8_t monoPalette[MAX_PALETTE_PIXELS / 8];
uint8_t colorPalette[MAX_PALETTE_PIXELS / 8];

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
} // namespace

bool drawBitmapFromSD(GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT> &display,
                      const char *path,
                      int16_t x,
                      int16_t y)
{
  if ((x >= int16_t(display.width())) || (y >= int16_t(display.height())))
  {
    return false;
  }

  File file = SD.open(path, FILE_READ);
  if (!file)
  {
    Serial.printf("BMP not found: %s\n", path);
    return false;
  }

  bool valid = false;
  bool flip = true;
  bool withColor = true;

  if (read16(file) == 0x4D42)
  {
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

    if ((planes == 1) && ((format == 0) || (format == 3)))
    {
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

      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= int16_t(display.width()))
      {
        w = int16_t(display.width()) - x;
      }
      if ((y + h - 1) >= int16_t(display.height()))
      {
        h = int16_t(display.height()) - y;
      }

      if (w <= MAX_ROW_WIDTH)
      {
        valid = true;
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

        display.setFullWindow();
        display.firstPage();
        do
        {
          display.fillScreen(GxEPD_WHITE);
          display.writeScreenBuffer();

          uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
          for (uint16_t row = 0; row < h; row++, rowPosition += rowSize)
          {
            uint32_t inRemain = rowSize;
            uint32_t inIdx = 0;
            uint32_t inBytes = 0;
            uint8_t inByte = 0;
            uint8_t inBits = 0;
            uint8_t outByte = 0xFF;
            uint8_t outColorByte = 0xFF;
            uint32_t outIdx = 0;

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
                break;
              }

              if (!whitish)
              {
                if (colored && withColor)
                {
                  outColorByte &= ~(0x80 >> (col % 8));
                }
                else
                {
                  outByte &= ~(0x80 >> (col % 8));
                }
              }

              if ((7 == col % 8) || (col == w - 1))
              {
                outputRowColor[outIdx] = outColorByte;
                outputRowMono[outIdx++] = outByte;
                outByte = 0xFF;
                outColorByte = 0xFF;
              }
            }

            const uint16_t yrow = y + (flip ? h - row - 1 : row);
            display.writeImage(outputRowMono, outputRowColor, x, yrow, w, 1);
          }
        } while (display.nextPage());
      }
    }
  }

  file.close();

  if (!valid)
  {
    Serial.printf("Unsupported BMP format: %s\n", path);
  }

  return valid;
}
