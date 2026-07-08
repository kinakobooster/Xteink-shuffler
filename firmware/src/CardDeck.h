#pragma once

#include <Arduino.h>
#include <SD.h>
#include <vector>

// Manages deck folders on the SD card root.
// Expected layout per deck:
//   /deckname/cover.bmp
//   /deckname/000.bmp
//   /deckname/001.bmp
class CardDeckManager
{
public:
  static constexpr size_t MAX_DECKS = 32;
  static constexpr size_t MAX_CARDS = 256;
  static constexpr size_t PATH_LEN = 64;

  bool scan();
  size_t deckCount() const { return deckNames_.size(); }
  size_t currentDeckIndex() const { return currentDeck_; }
  const char *currentDeckName() const;
  bool selectDeck(size_t index);
  bool nextDeck();
  bool previousDeck();

  bool hasCover() const;
  bool hasCards() const;
  size_t cardCount() const { return cardFiles_.size(); }

  bool coverPath(char *out, size_t outLen) const;
  bool randomCardPath(char *out, size_t outLen) const;
  bool randomCardPaths(char paths[][PATH_LEN], size_t pathLen, size_t count) const;

private:
  bool isCardFile(const char *name) const;
  bool inspectDeckFolder(const char *deckPath, bool *hasCover, size_t *cardCount) const;
  bool loadCardsForCurrentDeck();

  std::vector<String> deckNames_;
  std::vector<String> cardFiles_;
  size_t currentDeck_ = 0;
};
