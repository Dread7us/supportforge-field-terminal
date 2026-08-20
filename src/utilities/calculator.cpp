#include "calculator.h"

#include <math.h>

namespace utilities {
namespace {
constexpr size_t kMaximumInputCharacters = 24;
constexpr double kMaximumMagnitude = 1.0e15;
}

double Calculator::currentValue() const { return display_.toDouble(); }

void Calculator::setValue(double value) {
  if (!isfinite(value) || fabs(value) > kMaximumMagnitude) {
    display_ = "OUT OF RANGE";
    error_ = true;
    replaceInput_ = true;
    operation_ = 0;
    return;
  }
  char buffer[32]{};
  snprintf(buffer, sizeof(buffer), "%.10g", value);
  display_ = buffer;
  replaceInput_ = true;
}

void Calculator::inputDigit(char value) {
  if (error_) { display_ = "0"; error_ = false; }
  if (replaceInput_) { display_ = ""; replaceInput_ = false; }
  if (display_.length() >= kMaximumInputCharacters) return;
  if (value == '.' && display_.indexOf('.') >= 0) return;
  if (value == '.' && display_.isEmpty()) display_ = "0";
  if (display_ == "0" && value != '.') display_ = "";
  display_ += value;
}

void Calculator::applyPending() {
  if (!operation_) { accumulator_ = currentValue(); return; }
  const double right = currentValue();
  double result = accumulator_;
  if (operation_ == '+') result += right;
  else if (operation_ == '-') result -= right;
  else if (operation_ == '*') result *= right;
  else if (operation_ == '/') {
    if (fabs(right) < 1.0e-12) {
      display_ = "DIVIDE BY ZERO";
      error_ = true;
      operation_ = 0;
      replaceInput_ = true;
      return;
    }
    result /= right;
  }
  setValue(result);
  if (!error_) accumulator_ = result;
}

void Calculator::setOperator(char value) {
  if (error_) return;
  if (operation_ && !replaceInput_) applyPending();
  else if (!operation_) accumulator_ = currentValue();
  if (!error_) { operation_ = value; replaceInput_ = true; }
}

void Calculator::press(CalculatorKey key) {
  if (key >= CalculatorKey::Digit0 && key <= CalculatorKey::Digit9) {
    inputDigit(static_cast<char>('0' + static_cast<uint8_t>(key)));
    return;
  }
  switch (key) {
    case CalculatorKey::Decimal: inputDigit('.'); break;
    case CalculatorKey::Add: setOperator('+'); break;
    case CalculatorKey::Subtract: setOperator('-'); break;
    case CalculatorKey::Multiply: setOperator('*'); break;
    case CalculatorKey::Divide: setOperator('/'); break;
    case CalculatorKey::Equals:
      if (!error_) { applyPending(); operation_ = 0; replaceInput_ = true; }
      break;
    case CalculatorKey::Clear:
      display_ = "0"; accumulator_ = 0; operation_ = 0; replaceInput_ = true; error_ = false;
      break;
    case CalculatorKey::Backspace:
      if (error_) { display_ = "0"; error_ = false; }
      else if (!replaceInput_ && display_.length()) {
        display_.remove(display_.length() - 1);
        if (display_.isEmpty() || display_ == "-") display_ = "0";
      }
      break;
    case CalculatorKey::ToggleSign:
      if (!error_ && display_ != "0") {
        if (display_.startsWith("-")) display_.remove(0, 1); else display_ = "-" + display_;
      }
      break;
    default: break;
  }
}

String Calculator::fittedDisplay(size_t maximumCharacters) const {
  if (display_.length() <= maximumCharacters) return display_;
  if (maximumCharacters < 4) return display_.substring(display_.length() - maximumCharacters);
  return String("...") + display_.substring(display_.length() - (maximumCharacters - 3));
}

}  // namespace utilities