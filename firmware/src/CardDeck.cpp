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
} // namespace

bool CardDeckManager::isIgnoredDeckName(const char *name) const
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

bool CardDeckManager::loadCardsFromFolder(const char *deckPath, std::vector<String> &cards, bool *hasCover) const
{
  cards.clear();
  if (hasCover)
  {
    *hasCover = false;
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
      cards.push_back(String(base));
      if (cards.size() >= MAX_CARDS)
      {
        entry.close();
        break;
      }
    }

    entry.close();
  }

  dir.close();

  std::sort(cards.begin(), cards.end(),
            [](const String &a, const String &b) { return a.compareTo(b) < 0; });
  return true;
}

void CardDeckManager::applyCurrentDeck()
{
  if (currentDeck_ < deckCards_.size())
  {
    cardFiles_ = deckCards_[currentDeck_];
  }
  else
  {
    cardFiles_.clear();
  }
}

bool CardDeckManager::scan()
{
  deckNames_.clear();
  deckCards_.clear();
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
      entry.close();
      continue;
    }

    char deckPath[PATH_LEN];
    snprintf(deckPath, sizeof(deckPath), "/%s", base);

    bool hasCover = false;
    std::vector<String> cards;
    if (!loadCardsFromFolder(deckPath, cards, &hasCover))
    {
      entry.close();
      continue;
    }

    if (!hasCover && cards.empty())
    {
      entry.close();
      continue;
    }

    deckNames_.push_back(String(base));
    deckCards_.push_back(cards);
    Serial.printf("Deck cached: %s (cover=%d cards=%u)\n", base, hasCover,
                  static_cast<unsigned>(cards.size()));

    entry.close();
  }
  root.close();

  std::sort(deckNames_.begin(), deckNames_.end(),
            [](const String &a, const String &b) { return a.compareTo(b) < 0; });

  if (deckNames_.size() > MAX_DECKS)
  {
    deckNames_.resize(MAX_DECKS);
    deckCards_.resize(MAX_DECKS);
  }
  else if (deckCards_.size() > deckNames_.size())
  {
    deckCards_.resize(deckNames_.size());
  }

  if (deckNames_.empty())
  {
    return false;
  }

  for (size_t i = 0; i < deckNames_.size(); i++)
  {
    currentDeck_ = i;
    applyCurrentDeck();
    if (!cardFiles_.empty())
    {
      return true;
    }
  }

  currentDeck_ = 0;
  applyCurrentDeck();
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

const char *CardDeckManager::deckNameAt(size_t index) const
{
  if (index >= deckNames_.size())
  {
    return "";
  }
  return deckNames_[index].c_str();
}

size_t CardDeckManager::cardCountAt(size_t index) const
{
  if (index >= deckCards_.size())
  {
    return 0;
  }
  return deckCards_[index].size();
}

bool CardDeckManager::selectDeck(size_t index)
{
  if (index >= deckNames_.size())
  {
    return false;
  }
  currentDeck_ = index;
  applyCurrentDeck();
  return true;
}

bool CardDeckManager::nextDeck()
{
  if (deckNames_.empty())
  {
    return false;
  }
  currentDeck_ = (currentDeck_ + 1) % deckNames_.size();
  applyCurrentDeck();
  return true;
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
  applyCurrentDeck();
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

bool CardDeckManager::randomCardPaths(char paths[][PATH_LEN], size_t pathLen, size_t count) const
{
  if (!paths || pathLen == 0 || count == 0 || cardFiles_.empty() || deckNames_.empty())
  {
    return false;
  }

  const size_t available = cardFiles_.size();
  const size_t pickCount = count < available ? count : available;

  size_t indices[MAX_CARDS];
  for (size_t i = 0; i < available; i++)
  {
    indices[i] = i;
  }

  for (size_t i = 0; i < pickCount; i++)
  {
    const size_t j = i + static_cast<size_t>(random(available - i));
    const size_t tmp = indices[i];
    indices[i] = indices[j];
    indices[j] = tmp;
  }

  for (size_t i = 0; i < count; i++)
  {
    if (i < pickCount)
    {
      snprintf(paths[i], pathLen, "/%s/%s",
               deckNames_[currentDeck_].c_str(),
               cardFiles_[indices[i]].c_str());
    }
    else
    {
      snprintf(paths[i], pathLen, "/%s/%s",
               deckNames_[currentDeck_].c_str(),
               cardFiles_[indices[0]].c_str());
    }
  }

  return true;
}
