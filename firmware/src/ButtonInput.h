#pragma once

#include <Arduino.h>

enum Button : uint8_t
{
  BTN_NONE = 0,
  BTN_BACK,      // Button 1: show cover (shuffle)
  BTN_CONFIRM,   // Button 2: draw random card
  BTN_LEFT,      // Button 3: draw random card
  BTN_RIGHT,     // Button 4: draw random card
  BTN_UP,        // Deck previous
  BTN_DOWN,      // Deck next
  BTN_POWER
};

class ButtonInput
{
public:
  void begin();
  Button poll();
  bool wasPressed(Button button) const;
  const char *name(Button button) const;

private:
  static constexpr int THRESHOLD = 100;
  static constexpr int RIGHT_VAL = 3;
  static constexpr int LEFT_VAL = 1470;
  static constexpr int CONFIRM_VAL = 2655;
  static constexpr int BACK_VAL = 3470;
  static constexpr int DOWN_VAL = 3;
  static constexpr int UP_VAL = 2205;

  static constexpr int BTN_ADC1 = 1;
  static constexpr int BTN_ADC2 = 2;
  static constexpr int PIN_POWER = 3;

  Button readRaw() const;
  Button lastStable_ = BTN_NONE;
  Button lastRaw_ = BTN_NONE;
  unsigned long stableSinceMs_ = 0;
  static constexpr unsigned long DEBOUNCE_MS = 50;
};
