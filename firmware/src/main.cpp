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

bool showImage(const char *path)
{
  Serial.printf("Show: %s\n", path);
  if (drawBitmapFromSD(display, path))
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
  showImage(imagePath);
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
  showImage(imagePath);
}

void showDeckStatus()
{
  char line[CardDeckManager::PATH_LEN];
  snprintf(line, sizeof(line), "Deck: %s (%u/%u, %u cards)",
           decks.currentDeckName(),
           static_cast<unsigned>(decks.currentDeckIndex() + 1),
           static_cast<unsigned>(decks.deckCount()),
           static_cast<unsigned>(decks.cardCount()));
  showMessage(line, "Showing cover...");
  showCover();
}

void handleDeckChange(bool next)
{
  if (decks.deckCount() == 0)
  {
    showMessage("No decks on SD", "Create /name/cover.bmp");
    return;
  }

  if (next)
  {
    decks.nextDeck();
  }
  else
  {
    decks.previousDeck();
  }
  showDeckStatus();
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
  showDeckStatus();
}

void loop()
{
  const Button pressed = buttons.poll();
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
  case BTN_LEFT:
  case BTN_RIGHT:
    drawRandomCard();
    break;
  case BTN_UP:
    handleDeckChange(false);
    break;
  case BTN_DOWN:
    handleDeckChange(true);
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
