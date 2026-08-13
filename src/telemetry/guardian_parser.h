#pragma once

#include <stddef.h>

#include "telemetry_model.h"

namespace telemetry {

struct ParseResult {
  bool valid = false;
  uint16_t recognizedFields = 0;
};

ParseResult parseGuardianPayload(const char* payload, size_t length, Snapshot& output,
                                 const char* configuredHostName);

}  // namespace telemetry