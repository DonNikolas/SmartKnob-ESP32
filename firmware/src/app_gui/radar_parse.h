// Bounded parsing and local projection. ADS-B positions, not navigation data.
#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "radar.h"
namespace KnobRadar {
inline void cleanText(char* out, size_t size, const char* in) {
    size_t n = 0;
    if (in) while (*in && n + 1 < size) { unsigned char c = *in++; out[n++] = c >= 32 && c <= 126 ? c : '?'; }
    while (n && out[n-1] == ' ') --n;
    out[n] = 0;
}
inline bool parse(const char* body, size_t length, double lat, double lon, Snapshot& result) {
    StaticJsonDocument<512> filter;
    filter["now"] = true;
    for (const char* key : {"hex", "flight", "t", "lat", "lon", "seen_pos", "track", "alt_baro", "gs"}) filter["ac"][0][key] = true;
    DynamicJsonDocument doc(49152);
    if (deserializeJson(doc, body, length, DeserializationOption::Filter(filter))) return false;
    if (!doc["ac"].is<JsonArray>()) return false;
    result.sourceSeconds = doc["now"].as<double>() / 1000.0;
    result.count = 0;
    for (JsonObject a : doc["ac"].as<JsonArray>()) {
        if (!a["lat"].is<double>() || !a["lon"].is<double>() || !a["seen_pos"].is<double>()) continue;
        double la = a["lat"], lo = a["lon"], age = a["seen_pos"];
        if (!isfinite(la) || !isfinite(lo) || !isfinite(age) || fabs(la)>90 || fabs(lo)>180 || age<0 || age>15) continue;
        Aircraft p;
        double dl = lo-lon; if(dl>180)dl-=360; if(dl< -180)dl+=360;
        p.north = (la-lat)*111.195;
        p.east = dl*111.195*cos(lat*0.0174532925199433);
        p.distance = hypot(p.north,p.east);
        if (p.distance > 25) continue;
        cleanText(p.hex,sizeof(p.hex),a["hex"] | "");
        if (!p.hex[0]) continue;
        cleanText(p.callsign,sizeof(p.callsign),a["flight"] | "");
        if (!p.callsign[0]) cleanText(p.callsign,sizeof(p.callsign),p.hex);
        cleanText(p.type,sizeof(p.type),a["t"] | "");
        p.hasTrack = a["track"].is<double>() && isfinite(a["track"].as<double>());
        if(p.hasTrack) p.track = a["track"].as<float>();
        p.hasAltitude = a["alt_baro"].is<double>() && isfinite(a["alt_baro"].as<double>());
        if(p.hasAltitude) p.altitude = a["alt_baro"].as<float>()*0.3048f;
        p.hasSpeed = a["gs"].is<double>() && isfinite(a["gs"].as<double>());
        if(p.hasSpeed) p.speed = a["gs"].as<float>()*1.852f;
        unsigned index = 0; while(index < result.count && result.aircraft[index].distance <= p.distance) ++index;
        if(index >= MAX_AIRCRAFT) continue;
        if(result.count < MAX_AIRCRAFT) ++result.count;
        for(unsigned j=result.count-1;j>index;--j)result.aircraft[j]=result.aircraft[j-1];
        result.aircraft[index]=p;
    }
    return true;
}
}
