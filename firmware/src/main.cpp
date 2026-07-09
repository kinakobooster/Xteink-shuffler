#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <SD.h>
#include <esp_sleep.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#include "BmpSdDraw.h"
#include "ButtonInput.h"
#include "CardDeck.h"

namespace
{
constexpr uint32_t SPI_FREQ = 40000000;

constexpr int EPD_SCLK = 8;
constexpr int EPD_MOSI = 10;
constexpr int EPD_CS = 21;
constexpr int EPD_DC = 4;
constexpr int EPD_RST = 5;
constexpr int EPD_BUSY = 6;

constexpr int SD_CS = 12;
constexpr int SD_MISO = 7;

constexpr int PIN_POWER = 3;
constexpr unsigned long POWER_SLEEP_MS = 1000;

enum class AppMode : uint8_t
{
  Normal,
  DeckPicker,
};

AppMode appMode = AppMode::Normal;
size_t pickerIndex = 0;

GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT> display(
    GxEPD2_426_GDEQ0426T82(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

ButtonInput buttons;
CardDeckManager decks;
char imagePath[CardDeckManager::PATH_LEN];

void verifyWakeupLongPress()
{
  pinMode(PIN_POWER, INPUT_PULLUP);
  const unsigned long pressStart = millis();
  while (millis() - pressStart < POWER_SLEEP_MS)
  {
    if (digitalRead(PIN_POWER) == HIGH)
    {
      esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_POWER, ESP_GPIO_WAKEUP_GPIO_LOW);
      esp_deep_sleep_start();
    }
    delay(10);
  }
}

void enterDeepSleep()
{
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(120, 380);
    display.print("Sleeping...");
  } while (display.nextPage());

  delay(500);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_POWER, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void showMessage(const char *line1, const char *line2 = nullptr)
{
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(20, 120);
    display.print(line1);
    if (line2)
    {
      display.setCursor(20, 180);
      display.print(line2);
    }
  } while (display.nextPage());
}

bool showImage(const char *path, bool preferPartial)
{
  Serial.printf("Show: %s\n", path);
  if (drawBitmapFromSD(display, path, preferPartial, false))
  {
    return true;
  }

  showMessage("Image not found", path);
  return false;
}

void showCover()
{
  if (!decks.coverPath(imagePath, sizeof(imagePath)))
  {
    showMessage("No deck selected");
    return;
  }

  if (drawCachedCover(display, imagePath))
  {
    return;
  }

  if (!drawBitmapFromSD(display, imagePath, false, true))
  {
    showMessage("Image not found", imagePath);
  }
}

void drawRandomCard()
{
  if (!decks.randomCardPath(imagePath, sizeof(imagePath)))
  {
    char detail[CardDeckManager::PATH_LEN];
    snprintf(detail, sizeof(detail), "%s (%u cards)",
             decks.currentDeckName(),
             static_cast<unsigned>(decks.cardCount()));
    showMessage("No cards in deck", detail);
    return;
  }
  showImage(imagePath, true);
}

void drawThreeRandomCards()
{
  char paths[3][CardDeckManager::PATH_LEN];
  if (!decks.randomCardPaths(paths, CardDeckManager::PATH_LEN, 3))
  {
    char detail[CardDeckManager::PATH_LEN];
    snprintf(detail, sizeof(detail), "%s (%u cards)",
             decks.currentDeckName(),
             static_cast<unsigned>(decks.cardCount()));
    showMessage("Need 1+ cards", detail);
    return;
  }

  const char *ptrs[3] = {paths[0], paths[1], paths[2]};
  if (!drawThreeBitmapsFromSD(display, ptrs, true))
  {
    showMessage("Failed to draw 3", paths[0]);
  }
}

void formatDeckLabel(char *out, size_t outLen, const char *name, int16_t maxWidth)
{
  if (!out || outLen == 0)
  {
    return;
  }

  snprintf(out, outLen, "%s", name ? name : "");
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;

  while (out[0] != '\0')
  {
    display.getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
    if (w <= maxWidth)
    {
      return;
    }

    const size_t len = strlen(out);
    if (len <= 4)
    {
      out[0] = '\0';
      return;
    }

    out[len - 1] = '\0';
    if (strcmp(out + strlen(out) - 3, "...") != 0)
    {
      strlcat(out, "...", outLen);
    }
  }
}

void showDeckPicker(size_t highlightIndex)
{
  const size_t deckCount = decks.deckCount();
  if (deckCount == 0)
  {
    return;
  }

  constexpr int16_t LINE_H = 28;
  constexpr int16_t MARGIN_X = 16;
  constexpr int16_t PANEL_X = 8;
  constexpr int16_t TITLE_Y = 36;
  constexpr int16_t LIST_Y = 64;
  constexpr int16_t FOOTER_Y_OFFSET = 28;

  const int16_t screenW = display.width();
  const int16_t screenH = display.height();
  const int16_t textMaxW = screenW - 2 * MARGIN_X - 16;
  const size_t visibleLines = (screenH - LIST_Y - FOOTER_Y_OFFSET) / LINE_H;
  const size_t maxVisible = visibleLines > 0 ? visibleLines : 1;

  size_t scrollStart = 0;
  if (deckCount > maxVisible)
  {
    if (highlightIndex >= maxVisible / 2)
    {
      scrollStart = highlightIndex - (maxVisible / 2);
    }
    if (scrollStart + maxVisible > deckCount)
    {
      scrollStart = deckCount - maxVisible;
    }
  }

  const int16_t panelW = screenW - 2 * PANEL_X;
  const int16_t panelH = static_cast<int16_t>(screenH - 24);

  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(PANEL_X, 12, panelW, panelH, GxEPD_BLACK);
    display.setFont(&FreeMono9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN_X, TITLE_Y);
    display.print("Deck ");
    display.print(static_cast<unsigned>(highlightIndex + 1));
    display.print("/");
    display.print(static_cast<unsigned>(deckCount));

    char label[CardDeckManager::PATH_LEN];
    for (size_t row = 0; row < maxVisible && scrollStart + row < deckCount; row++)
    {
      const size_t idx = scrollStart + row;
      const int16_t y = static_cast<int16_t>(LIST_Y + row * LINE_H);
      const bool selected = idx == highlightIndex;

      formatDeckLabel(label, sizeof(label), decks.deckNameAt(idx), textMaxW);

      if (selected)
      {
        display.fillRect(MARGIN_X - 4, y - 18, panelW - 16, LINE_H - 2, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(MARGIN_X, y);
        display.print("> ");
        display.print(label);
      }
      else
      {
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(MARGIN_X + 14, y);
        display.print(label);
      }
    }

    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN_X, screenH - FOOTER_Y_OFFSET);
    display.print("OK=select  Back=cancel");
  } while (display.nextPage());
}

void confirmDeckPicker()
{
  appMode = AppMode::Normal;

  if (pickerIndex != decks.currentDeckIndex())
  {
    invalidateCoverCache();
    decks.selectDeck(pickerIndex);
    Serial.printf("Deck selected: %s (%u/%u, %u cards)\n",
                  decks.currentDeckName(),
                  static_cast<unsigned>(decks.currentDeckIndex() + 1),
                  static_cast<unsigned>(decks.deckCount()),
                  static_cast<unsigned>(decks.cardCount()));
    showCover();
    return;
  }

  Serial.printf("Deck unchanged: %s\n", decks.currentDeckName());
  showCover();
}

void cancelDeckPicker()
{
  appMode = AppMode::Normal;
  pickerIndex = decks.currentDeckIndex();
  showCover();
}

void moveDeckPicker(bool next)
{
  const size_t deckCount = decks.deckCount();
  if (deckCount == 0)
  {
    showMessage("No decks on SD", "Create /name/cover.bmp");
    return;
  }

  if (deckCount == 1)
  {
    showCover();
    return;
  }

  if (next)
  {
    pickerIndex = (pickerIndex + 1) % deckCount;
  }
  else if (pickerIndex == 0)
  {
    pickerIndex = deckCount - 1;
  }
  else
  {
    pickerIndex--;
  }

  showDeckPicker(pickerIndex);
}

void openDeckPicker()
{
  const size_t deckCount = decks.deckCount();
  if (deckCount == 0)
  {
    showMessage("No decks on SD", "Create /name/cover.bmp");
    return;
  }

  if (deckCount == 1)
  {
    showCover();
    return;
  }

  appMode = AppMode::DeckPicker;
  pickerIndex = decks.currentDeckIndex();
  showDeckPicker(pickerIndex);
}
} // namespace

void setup()
{
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO)
  {
    verifyWakeupLongPress();
  }

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("Xteink Card Shuffler");

  buttons.begin();
  randomSeed(esp_random());

  SPI.begin(EPD_SCLK, SD_MISO, EPD_MOSI, EPD_CS);
  SPISettings spiSettings(SPI_FREQ, MSBFIRST, SPI_MODE0);
  display.init(115200, true, 2, false, SPI, spiSettings);
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);

  if (!SD.begin(SD_CS, SPI, SPI_FREQ))
  {
    showMessage("SD card not found", "Insert FAT32 microSD");
    return;
  }

  if (!decks.scan())
  {
    showMessage("No decks found", "Add folders like /aaa/");
    return;
  }

  Serial.printf("Found %u deck(s), first: %s (%u cards)\n",
                static_cast<unsigned>(decks.deckCount()),
                decks.currentDeckName(),
                static_cast<unsigned>(decks.cardCount()));
  showCover();
}

void loop()
{
  const Button pressed = buttons.poll();

  if (appMode == AppMode::DeckPicker)
  {
    if (pressed == BTN_NONE)
    {
      delay(20);
      return;
    }

    Serial.printf("Picker button: %s\n", buttons.name(pressed));

    switch (pressed)
    {
    case BTN_UP:
      moveDeckPicker(false);
      break;
    case BTN_DOWN:
      moveDeckPicker(true);
      break;
    case BTN_BACK:
      cancelDeckPicker();
      break;
    case BTN_CONFIRM:
      confirmDeckPicker();
      break;
    case BTN_LEFT:
    case BTN_RIGHT:
      cancelDeckPicker();
      drawRandomCard();
      break;
    case BTN_POWER:
    {
      const unsigned long start = millis();
      while (digitalRead(PIN_POWER) == LOW)
      {
        delay(50);
      }
      if (millis() - start > POWER_SLEEP_MS)
      {
        enterDeepSleep();
      }
      break;
    }
    default:
      break;
    }

    delay(80);
    return;
  }

  if (pressed == BTN_NONE)
  {
    delay(20);
    return;
  }

  Serial.printf("Button: %s\n", buttons.name(pressed));

  switch (pressed)
  {
  case BTN_BACK:
    showCover();
    break;
  case BTN_CONFIRM:
    drawThreeRandomCards();
    break;
  case BTN_LEFT:
  case BTN_RIGHT:
    drawRandomCard();
    break;
  case BTN_UP:
  case BTN_DOWN:
    openDeckPicker();
    break;
  case BTN_POWER:
  {
    const unsigned long start = millis();
    while (digitalRead(PIN_POWER) == LOW)
    {
      delay(50);
    }
    if (millis() - start > POWER_SLEEP_MS)
    {
      enterDeepSleep();
    }
    break;
  }
  default:
    break;
  }

  delay(80);
}
