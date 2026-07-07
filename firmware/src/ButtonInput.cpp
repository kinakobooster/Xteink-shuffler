#include "ButtonInput.h"

void ButtonInput::begin()
{
  pinMode(BTN_ADC1, INPUT);
  pinMode(BTN_ADC2, INPUT);
  pinMode(PIN_POWER, INPUT_PULLUP);
}

Button ButtonInput::readRaw() const
{
  const int btn1 = analogRead(BTN_ADC1);
  const int btn2 = analogRead(BTN_ADC2);

  if (digitalRead(PIN_POWER) == LOW)
  {
    return BTN_POWER;
  }

  if (btn1 < RIGHT_VAL + THRESHOLD)
  {
    return BTN_RIGHT;
  }
  if (btn1 < LEFT_VAL + THRESHOLD)
  {
    return BTN_LEFT;
  }
  if (btn1 < CONFIRM_VAL + THRESHOLD)
  {
    return BTN_CONFIRM;
  }
  if (btn1 < BACK_VAL + THRESHOLD)
  {
    return BTN_BACK;
  }

  if (btn2 < DOWN_VAL + THRESHOLD)
  {
    return BTN_DOWN;
  }
  if (btn2 < UP_VAL + THRESHOLD)
  {
    return BTN_UP;
  }

  return BTN_NONE;
}

Button ButtonInput::poll()
{
  const Button raw = readRaw();
  const unsigned long now = millis();

  if (raw != lastRaw_)
  {
    lastRaw_ = raw;
    stableSinceMs_ = now;
    return BTN_NONE;
  }

  if (raw != BTN_NONE && raw != lastStable_ && (now - stableSinceMs_) >= DEBOUNCE_MS)
  {
    lastStable_ = raw;
    return raw;
  }

  if (raw == BTN_NONE)
  {
    lastStable_ = BTN_NONE;
  }

  return BTN_NONE;
}

bool ButtonInput::wasPressed(Button button) const
{
  return button != BTN_NONE;
}

const char *ButtonInput::name(Button button) const
{
  switch (button)
  {
  case BTN_BACK:
    return "BACK";
  case BTN_CONFIRM:
    return "CONFIRM";
  case BTN_LEFT:
    return "LEFT";
  case BTN_RIGHT:
    return "RIGHT";
  case BTN_UP:
    return "UP";
  case BTN_DOWN:
    return "DOWN";
  case BTN_POWER:
    return "POWER";
  default:
    return "NONE";
  }
}
