#pragma once
// Copy this file to firmware/include/mqtt_private/config.h for local settings.
// That path is ignored by Git. Keep this template free of real credentials.
namespace KnobMQTTConfig {
constexpr bool ENABLED=false;
constexpr char HOST[]=""; // Broker IP or hostname, no mqtt:// prefix.
constexpr unsigned PORT=1883;
constexpr bool TLS=false;
constexpr char CA_CERT[]=""; // PEM CA certificate required if TLS=true.
constexpr char USER[]="";
constexpr char PASSWORD[]="";
constexpr char TEMPERATURE_TOPIC[]="smartknob/sensors/temperature";
constexpr char INPUT_TOPIC[]="smartknob/input/value";
constexpr char STATUS_TOPIC[]="smartknob/status";
constexpr char INPUT_LABEL[]="MQTT Value";
constexpr char INPUT_UNIT[]="";
constexpr unsigned INPUT_DECIMALS=1;
constexpr unsigned PUBLISH_MS=10000;
constexpr unsigned RECEIVE_MAX_AGE_MS=300000;
}
