#include "CardDeck.h"

#include <algorithm>
#include <ctype.h>
#include <string.h>

bool CardDeckManager::isCardFile(const char *name) const
{
  if (!name)
  {
    return false;
  }

  const size_t len = strlen(name);
  if (len != 7 || strcasecmp(name + 4, ".bmp") != 0)
  {
    return false;
  }

  for (int i = 0; i < 3; i++)
  {
    if (!isdigit(static_cast<unsigned char>(name[i])))
    {
      return false;
    }
  }

  return true;
}

bool CardDeckManager::scan()
{
  deckNames_.clear();
  cardFiles_.clear();
  currentDeck_ = 0;

  File root = SD.open("/");
  if (!root || !root.isDirectory())
  {
    if (root)
    {
      root.close();
    }
    return false;
  }

  for (File entry = root.openNextFile(); entry; entry = root.openNextFile())
  {
    if (entry.isDirectory())
    {
      const char *name = entry.name();
      const char *slash = strrchr(name, '/');
      const char *basename = slash ? slash + 1 : name;
      if (basename && basename[0] != '\0' && basename[0] != '.')
      {
        deckNames_.push_back(String(basename));
      }
    }
    entry.close();
  }
  root.close();

  std::sort(deckNames_.begin(), deckNames_.end(),
            [](const String &a, const String &b) { return a.compareTo(b) < 0; });

  if (deckNames_.size() > MAX_DECKS)
  {
    deckNames_.resize(MAX_DECKS);
  }

  if (!deckNames_.empty())
  {
    loadCardsForCurrentDeck();
  }

  return !deckNames_.empty();
}

const char *CardDeckManager::currentDeckName() const
{
  if (deckNames_.empty())
  {
    return "";
  }
  return deckNames_[currentDeck_].c_str();
}

bool CardDeckManager::selectDeck(size_t index)
{
  if (index >= deckNames_.size())
  {
    return false;
  }
  currentDeck_ = index;
  return loadCardsForCurrentDeck();
}

bool CardDeckManager::nextDeck()
{
  if (deckNames_.empty())
  {
    return false;
  }
  currentDeck_ = (currentDeck_ + 1) % deckNames_.size();
  return loadCardsForCurrentDeck();
}

bool CardDeckManager::previousDeck()
{
  if (deckNames_.empty())
  {
    return false;
  }
  if (currentDeck_ == 0)
  {
    currentDeck_ = deckNames_.size() - 1;
  }
  else
  {
    currentDeck_--;
  }
  return loadCardsForCurrentDeck();
}

bool CardDeckManager::loadCardsForCurrentDeck()
{
  cardFiles_.clear();

  if (deckNames_.empty())
  {
    return false;
  }

  const String deckPath = "/" + deckNames_[currentDeck_];
  File dir = SD.open(deckPath.c_str());
  if (!dir || !dir.isDirectory())
  {
    if (dir)
    {
      dir.close();
    }
    return false;
  }

  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile())
  {
    if (!entry.isDirectory())
    {
      const char *name = entry.name();
      const char *slash = strrchr(name, '/');
      const char *basename = slash ? slash + 1 : name;
      if (isCardFile(basename))
      {
        cardFiles_.push_back(String(basename));
      }
    }
    entry.close();

    if (cardFiles_.size() >= MAX_CARDS)
    {
      break;
    }
  }
  dir.close();

  std::sort(cardFiles_.begin(), cardFiles_.end(),
            [](const String &a, const String &b) { return a.compareTo(b) < 0; });
  return true;
}

bool CardDeckManager::hasCover() const
{
  if (deckNames_.empty())
  {
    return false;
  }

  char path[PATH_LEN];
  return coverPath(path, sizeof(path)) && SD.exists(path);
}

bool CardDeckManager::hasCards() const
{
  return !cardFiles_.empty();
}

bool CardDeckManager::coverPath(char *out, size_t outLen) const
{
  if (!out || outLen == 0 || deckNames_.empty())
  {
    return false;
  }

  snprintf(out, outLen, "/%s/cover.bmp", deckNames_[currentDeck_].c_str());
  return true;
}

bool CardDeckManager::randomCardPath(char *out, size_t outLen) const
{
  if (!out || outLen == 0 || cardFiles_.empty() || deckNames_.empty())
  {
    return false;
  }

  const size_t index = static_cast<size_t>(random(cardFiles_.size()));
  snprintf(out, outLen, "/%s/%s", deckNames_[currentDeck_].c_str(), cardFiles_[index].c_str());
  return true;
}
