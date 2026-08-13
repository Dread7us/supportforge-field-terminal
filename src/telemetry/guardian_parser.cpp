#include "guardian_parser.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdlib.h>

namespace telemetry {
namespace {

bool boundedText(JsonVariantConst value, TextValue& out) {
  if (!value.is<const char*>()) return false;
  const char* text = value.as<const char*>();
  if (!text) return false;
  const size_t length = strnlen(text, appconfig::kMaximumStringLength + 1);
  if (length > appconfig::kMaximumStringLength) return false;
  memcpy(out.value, text, length);
  out.value[length] = '\0';
  out.available = true;
  return true;
}

bool finiteNumber(JsonVariantConst value, NumericValue& out, bool nonnegative = false) {
  if (!value.is<float>() && !value.is<double>() && !value.is<int>() &&
      !value.is<long>() && !value.is<unsigned int>() && !value.is<unsigned long>() &&
      !value.is<long long>() && !value.is<unsigned long long>()) return false;
  const double number = value.as<double>();
  if (!isfinite(number) || (nonnegative && number < 0.0)) return false;
  out.available = true;
  out.value = number;
  return true;
}

bool nonnegativeInteger(JsonVariantConst value, uint64_t& out) {
  NumericValue number;
  if (!finiteNumber(value, number, true) || number.value > 18446744073709549568.0) return false;
  out = static_cast<uint64_t>(number.value);
  return true;
}

void recognizeText(JsonObjectConst root, const char* key, TextValue& out, uint16_t& count) {
  if (boundedText(root[key], out)) ++count;
}

void recognizeNumber(JsonObjectConst root, const char* key, NumericValue& out,
                     uint16_t& count, bool nonnegative = false) {
  if (finiteNumber(root[key], out, nonnegative)) ++count;
}

}  // namespace

ParseResult parseGuardianPayload(const char* payload, size_t length, Snapshot& output,
                                 const char* configuredHostName) {
  ParseResult result;
  if (!payload || !length || length > appconfig::kMaximumResponseBytes) return result;
  JsonDocument document;
  const DeserializationError error = deserializeJson(
      document, payload, length, DeserializationOption::NestingLimit(8));
  if (error || !document.is<JsonObject>()) return result;
  const JsonObjectConst root = document.as<JsonObjectConst>();
  Snapshot parsed;
  parsed.displayTemperatureUnit = output.displayTemperatureUnit;
  uint16_t& count = result.recognizedFields;

  recognizeNumber(root, "cpu_load", parsed.cpuLoad, count, true);
  recognizeNumber(root, "cpu_temp", parsed.cpuTemperature, count);
  recognizeNumber(root, "ram_used_gb", parsed.ramUsedGb, count, true);
  recognizeNumber(root, "ram_total_gb", parsed.ramTotalGb, count, true);
  recognizeNumber(root, "nvme_temp", parsed.nvmeTemperature, count);
  if (nonnegativeInteger(root["uptime_seconds"], parsed.uptimeSeconds)) {
    parsed.uptimeAvailable = true;
    ++count;
  }
  if (boundedText(root["system_status"], parsed.systemStatus)) {
    parsed.explicitSystemStatus = true;
    ++count;
  }

  const char* hostKeys[] = {"hostname", "host", "device_name", "deviceName", "name", "machine"};
  for (const char* key : hostKeys) {
    if (boundedText(root[key], parsed.host)) break;
  }
  if (!parsed.host.available && configuredHostName) {
    const size_t n = strnlen(configuredHostName, appconfig::kMaximumStringLength + 1);
    if (n <= appconfig::kMaximumStringLength) {
      memcpy(parsed.host.value, configuredHostName, n + 1);
      parsed.host.available = true;
    }
  }

  if (parsed.ramUsedGb.available && parsed.ramTotalGb.available) {
    if (parsed.ramTotalGb.value <= 0.0 ||
        parsed.ramUsedGb.value > parsed.ramTotalGb.value * 1.05) return result;
    parsed.ramPercent.available = true;
    parsed.ramPercent.value = parsed.ramUsedGb.value * 100.0 / parsed.ramTotalGb.value;
  }

  if (root["disks"].is<JsonArrayConst>()) {
    for (JsonObjectConst disk : root["disks"].as<JsonArrayConst>()) {
      if (parsed.diskCount >= appconfig::kMaximumDisks) break;
      Disk candidate;
      bool recognized = false;
      recognized |= boundedText(disk["fs"], candidate.fs);
      recognized |= boundedText(disk["mount"], candidate.mount);
      recognized |= (candidate.sizeAvailable = nonnegativeInteger(disk["sizeBytes"], candidate.sizeBytes));
      recognized |= (candidate.usedAvailable = nonnegativeInteger(disk["usedBytes"], candidate.usedBytes));
      recognized |= (candidate.availableBytesAvailable = nonnegativeInteger(disk["availableBytes"], candidate.availableBytes));
      if (finiteNumber(disk["usedPercent"], candidate.usedPercent, true)) {
        if (candidate.usedPercent.value > 100.5) return result;
        recognized = true;
      }
      if (candidate.sizeAvailable && candidate.usedAvailable && candidate.usedBytes > candidate.sizeBytes) return result;
      if (recognized) {
        parsed.disks[parsed.diskCount++] = candidate;
        ++count;
      }
    }
  }

  if (root["speedtest"].is<JsonObjectConst>()) {
    const JsonObjectConst speed = root["speedtest"].as<JsonObjectConst>();
    recognizeNumber(speed, "down", parsed.speedTest.down, count, true);
    recognizeNumber(speed, "up", parsed.speedTest.up, count, true);
    recognizeNumber(speed, "ping", parsed.speedTest.ping, count, true);
    recognizeText(speed, "last_run", parsed.speedTest.lastRun, count);
    recognizeText(speed, "status", parsed.speedTest.status, count);
    recognizeText(speed, "started_at", parsed.speedTest.startedAt, count);
    recognizeText(speed, "error", parsed.speedTest.error, count);
    recognizeText(speed, "provider", parsed.speedTest.provider, count);
    if (speed["is_running"].is<bool>()) {
      parsed.speedTest.isRunningAvailable = true;
      parsed.speedTest.isRunning = speed["is_running"].as<bool>();
      ++count;
    }
  }

  if (!count) return result;
  parsed.recognizedFields = count;
  parsed.optionalDataMissing = !(parsed.cpuLoad.available && parsed.cpuTemperature.available &&
      parsed.ramUsedGb.available && parsed.ramTotalGb.available && parsed.diskCount &&
      parsed.nvmeTemperature.available && parsed.uptimeAvailable &&
      parsed.speedTest.down.available && parsed.speedTest.up.available && parsed.speedTest.ping.available);
  output = parsed;
  result.valid = true;
  return result;
}

}  // namespace telemetry