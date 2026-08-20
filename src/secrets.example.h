#pragma once

#define SUPPORTFORGE_PRIMARY_TELEMETRY_URL \
    "http://YOUR_PRIMARY_SERVER_IP:PORT/api/v1/admin/guardian/telemetry"
#define SUPPORTFORGE_FALLBACK_TELEMETRY_URL \
    "http://YOUR_FALLBACK_SERVER_IP:PORT/api/v1/admin/guardian/telemetry"
#define SUPPORTFORGE_GUARDIAN_TOKEN "YOUR_DEVICE_SPECIFIC_TOKEN"
#define SUPPORTFORGE_TARGET_HOST_NAME "YOUR_MONITORED_HOST"

// Optional HOME weather card. Copy only the values you choose into the ignored
// src/secrets.h. Coordinates and generated request URLs are never logged.
#define SUPPORTFORGE_WEATHER_LATITUDE "YOUR_LATITUDE"
#define SUPPORTFORGE_WEATHER_LONGITUDE "YOUR_LONGITUDE"
#define SUPPORTFORGE_WEATHER_CITY_LABEL "YOUR_CITY"
// "C" (default) or "F".
#define SUPPORTFORGE_WEATHER_UNIT "C"