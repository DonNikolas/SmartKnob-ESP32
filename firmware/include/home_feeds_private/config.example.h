#pragma once
// Copy to config.h and enter the complete HTTPS URL for each page.
// Order: freezer, humidity, garage temperature, garage light, air pressure.
namespace KnobHomeConfig {
static constexpr const char* URLS[5] = {"", "", "", "", ""};
// Optional exact known server warning before JSON. Leave empty otherwise.
static constexpr char WARNING_PREFIX[] = "";
}
