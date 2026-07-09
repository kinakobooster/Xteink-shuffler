#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <SD.h>
#include <esp_sleep.h>
#include <Fonts/FreeMonoBold12pt7b.h>

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
constexpr unsigned long DECK_PICKER_DWELL_MS = 400;

enum class AppMode : uint8_t
{
  Normal,
  DeckPicker,
};

AppMode appMode = AppMode::Normal;
size_t pickerIndex = 0;
unsigned long pickerMovedAt = 0;

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

bool randomCardInverted()
{
  return random(2) == 0;
}

bool showImage(const char *path, bool preferPartial, bool flipVertical = false)
{
  Serial.printf("Show: %s%s\n", path, flipVertical ? " (inverted)" : "");
  if (drawBitmapFromSD(display, path, preferPartial, false, flipVertical))
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
  showImage(imagePath, true, randomCardInverted());
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
  const bool flips[3] = {randomCardInverted(), randomCardInverted(), randomCardInverted()};
  if (!drawThreeBitmapsFromSD(display, ptrs, true, flips))
  {
    showMessage("Failed to draw 3", paths[0]);
  }
}

void showDeckPicker(size_t highlightIndex)
{
  const size_t deckCount = decks.deckCount();
  if (deckCount == 0)
  {
    return;
  }

  constexpr int16_t LINE_H = 38;
  constexpr size_t VISIBLE_LINES = 7;
  constexpr int16_t MARGIN_X = 20;
  constexpr int16_t PANEL_X = 12;
  constexpr int16_t TITLE_Y = 56;
  constexpr int16_t LIST_Y = 96;

  size_t scrollStart = 0;
  if (deckCount > VISIBLE_LINES)
  {
    if (highlightIndex >= VISIBLE_LINES / 2)
    {
      scrollStart = highlightIndex - (VISIBLE_LINES / 2);
    }
    if (scrollStart + VISIBLE_LINES > deckCount)
    {
      scrollStart = deckCount - VISIBLE_LINES;
    }
  }

  const int16_t panelW = display.width() - 2 * PANEL_X;
  const int16_t panelH = static_cast<int16_t>(LIST_Y - 20 + VISIBLE_LINES * LINE_H + 16);

  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(PANEL_X, 16, panelW, panelH, GxEPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN_X, TITLE_Y);
    display.print("Deck");

    for (size_t row = 0; row < VISIBLE_LINES && scrollStart + row < deckCount; row++)
    {
      const size_t idx = scrollStart + row;
      const int16_t y = static_cast<int16_t>(LIST_Y + row * LINE_H);
      const bool selected = idx == highlightIndex;

      if (selected)
      {
        display.fillRect(MARGIN_X - 4, y - 28, panelW - 16, LINE_H - 4, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
      }
      else
      {
        display.setTextColor(GxEPD_BLACK);
      }

      display.setCursor(MARGIN_X + 4, y);
      display.print(decks.deckNameAt(idx));

      char suffix[16];
      snprintf(suffix, sizeof(suffix), " (%u)", static_cast<unsigned>(decks.cardCountAt(idx)));
      display.print(suffix);
    }

    display.setTextColor(GxEPD_BLACK);
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

  pickerMovedAt = millis();
  showDeckPicker(pickerIndex);
}

void openDeckPicker(bool next)
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
  moveDeckPicker(next);
}

bool deckPickerDwellElapsed()
{
  return appMode == AppMode::DeckPicker &&
         (millis() - pickerMovedAt) >= DECK_PICKER_DWELL_MS;
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
  if (deckPickerDwellElapsed())
  {
    confirmDeckPicker();
    delay(80);
    return;
  }

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
      cancelDeckPicker();
      drawThreeRandomCards();
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
    openDeckPicker(false);
    break;
  case BTN_DOWN:
    openDeckPicker(true);
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
