#include "CardDeck.h"

#include <algorithm>
#include <ctype.h>
#include <string.h>

namespace
{
const char *basenameOf(const char *path)
{
  if (!path)
  {
    return "";
  }

  const char *slash = strrchr(path, '/');
  if (!slash)
  {
    slash = strrchr(path, '\\');
  }
  return slash ? slash + 1 : path;
}

bool isCoverName(const char *name)
{
  return name && strcasecmp(name, "cover.bmp") == 0;
}

bool isIgnoredDeckName(const char *name)
{
  if (!name || name[0] == '\0')
  {
    return true;
  }

  if (name[0] == '.' || name[0] == '_' || name[0] == '$')
  {
    return true;
  }

  static const char *IGNORED[] = {
      "System Volume Information",
      "__MACOSX",
      "LOST.DIR",
      "Android",
      "RECYCLED",
      "RECYCLER",
  };

  for (const char *ignored : IGNORED)
  {
    if (strcasecmp(name, ignored) == 0)
    {
      return true;
    }
  }

  return false;
}
} // namespace

bool CardDeckManager::isCardFile(const char *name) const
{
  if (!name || name[0] == '\0' || name[0] == '.')
  {
    return false;
  }

  if (isCoverName(name))
  {
    return false;
  }

  const size_t len = strlen(name);
  if (len < 5 || strcasecmp(name + len - 4, ".bmp") != 0)
  {
    return false;
  }

  const size_t stemLen = len - 4;
  if (stemLen >= 1 && stemLen <= 3)
  {
    bool allDigits = true;
    for (size_t i = 0; i < stemLen; i++)
    {
      if (!isdigit(static_cast<unsigned char>(name[i])))
      {
        allDigits = false;
        break;
      }
    }
    if (allDigits)
    {
      return true;
    }
  }

  return true;
}

bool CardDeckManager::inspectDeckFolder(const char *deckPath, bool *hasCover, size_t *cardCount) const
{
  if (hasCover)
  {
    *hasCover = false;
  }
  if (cardCount)
  {
    *cardCount = 0;
  }

  File dir = SD.open(deckPath);
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
    if (entry.isDirectory())
    {
      entry.close();
      continue;
    }

    const char *base = basenameOf(entry.name());
    if (isCoverName(base))
    {
      if (hasCover)
      {
        *hasCover = true;
      }
    }
    else if (isCardFile(base))
    {
      if (cardCount)
      {
        (*cardCount)++;
      }
    }

    entry.close();
  }

  dir.close();
  return true;
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
    Serial.printf("Deck open failed: %s\n", deckPath.c_str());
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
      const char *base = basenameOf(entry.name());
      Serial.printf("  file: %s\n", base);
      if (isCardFile(base))
      {
        cardFiles_.push_back(String(base));
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

  Serial.printf("Deck %s: %u card(s)\n", deckNames_[currentDeck_].c_str(),
                static_cast<unsigned>(cardFiles_.size()));
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
    if (!entry.isDirectory())
    {
      entry.close();
      continue;
    }

    const char *base = basenameOf(entry.name());
    if (isIgnoredDeckName(base))
    {
      Serial.printf("Skip folder: %s\n", base);
      entry.close();
      continue;
    }

    char deckPath[PATH_LEN];
    snprintf(deckPath, sizeof(deckPath), "/%s", base);

    bool hasCover = false;
    size_t cardCount = 0;
    if (inspectDeckFolder(deckPath, &hasCover, &cardCount) && (hasCover || cardCount > 0))
    {
      deckNames_.push_back(String(base));
      Serial.printf("Deck found: %s (cover=%d cards=%u)\n", base, hasCover,
                    static_cast<unsigned>(cardCount));
    }
    else
    {
      Serial.printf("Skip empty folder: %s\n", base);
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

  if (deckNames_.empty())
  {
    return false;
  }

  for (size_t i = 0; i < deckNames_.size(); i++)
  {
    currentDeck_ = i;
    loadCardsForCurrentDeck();
    if (!cardFiles_.empty())
    {
      return true;
    }
  }

  currentDeck_ = 0;
  loadCardsForCurrentDeck();
  return true;
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
