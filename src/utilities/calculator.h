#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace utilities {

enum class CalculatorKey : uint8_t {
  Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
  Decimal, Add, Subtract, Multiply, Divide, Equals, Clear, Backspace, ToggleSign
};

class Calculator {
 public:
  void press(CalculatorKey key);
  const String& display() const { return display_; }
  String fittedDisplay(size_t maximumCharacters) const;
  bool error() const { return error_; }

 private:
  void inputDigit(char value);
  void applyPending();
  void setOperator(char value);
  void setValue(double value);
  double currentValue() const;

  String display_ = "0";
  double accumulator_ = 0.0;
  char operation_ = 0;
  bool replaceInput_ = true;
  bool error_ = false;
};

}  // namespace utilities