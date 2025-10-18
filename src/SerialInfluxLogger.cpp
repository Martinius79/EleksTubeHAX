#define INFLUX_SERIAL_IMPLEMENTATION
#include "_USER_DEFINES.h"
#include "SerialInfluxLogger.h"

#ifdef ENABLE_INFLUX_SERIAL_LOGGER

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

extern char UniqueDeviceName[32];

namespace {

String trimSlash(String value) {
    while (value.endsWith("/")) {
        value.remove(value.length() - 1);
    }
    return value;
}

String escapeTag(const String &input) {
    String out;
    out.reserve(input.length());
    for (uint8_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        if (c == ',' || c == ' ' || c == '=') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

String escapeField(const String &input) {
    String out;
    out.reserve(input.length());
    for (uint8_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

String urlEncode(const String &value) {
    String encoded;
    encoded.reserve(value.length() * 3);
    const char *hex = "0123456789ABCDEF";
    for (uint8_t i = 0; i < value.length(); ++i) {
        char c = value[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            encoded += '%';
            encoded += hex[(c >> 4) & 0x0F];
            encoded += hex[c & 0x0F];
        }
    }
    return encoded;
}

String detectLevel(const String &line) {
#if SERIAL_INFLUX_INCLUDE_LEVEL
    String upper = line;
    upper.toUpperCase();
    if (upper.indexOf("FATAL") >= 0) return "fatal";
    if (upper.indexOf("CRITICAL") >= 0) return "critical";
    if (upper.indexOf("ERROR") >= 0) return "error";
    if (upper.indexOf("WARN") >= 0) return "warn";
    if (upper.indexOf("INFO") >= 0) return "info";
    if (upper.indexOf("DEBUG") >= 0) return "debug";
    if (upper.indexOf("TRACE") >= 0) return "trace";
#endif
    return String(SERIAL_INFLUX_DEFAULT_LEVEL);
}

bool isSecureUrl(const String &url) {
    return url.startsWith("https://");
}

} // namespace

namespace influxserial_detail {

HardwareSerial &hardwareSerial();

} // namespace influxserial_detail

InfluxLineWriter::InfluxLineWriter() {
    if (strlen(SERIAL_INFLUX_URL) == 0) {
        enabled_ = false;
        return;
    }

    baseUrl_ = trimSlash(String(SERIAL_INFLUX_URL));
    secure_ = isSecureUrl(baseUrl_);
    measurement_ = String(SERIAL_INFLUX_MEASUREMENT);

    String tags;
    if (strlen(SERIAL_INFLUX_DEVICE_TAG) > 0) {
        tags += "device=";
        tags += escapeTag(String(SERIAL_INFLUX_DEVICE_TAG));
    } else {
        useUniqueDeviceTag_ = true;
    }
    if (strlen(SERIAL_INFLUX_SOURCE_TAG) > 0) {
        if (!tags.isEmpty()) tags += ',';
        tags += "source=";
        tags += escapeTag(String(SERIAL_INFLUX_SOURCE_TAG));
    }
    staticTags_ = tags;

    enabled_ = buildWriteUrl();
}

bool InfluxLineWriter::buildWriteUrl() {
#if SERIAL_INFLUX_VERSION == 2
    if (strlen(SERIAL_INFLUX_BUCKET) == 0 || strlen(SERIAL_INFLUX_ORG) == 0 || strlen(SERIAL_INFLUX_TOKEN) == 0) {
        return false;
    }
    String query = String("?org=") + urlEncode(String(SERIAL_INFLUX_ORG)) + "&bucket=" + urlEncode(String(SERIAL_INFLUX_BUCKET)) + "&precision=ns";
    writeUrl_ = baseUrl_ + "/api/v2/write" + query;
    authHeader_ = String("Token ") + SERIAL_INFLUX_TOKEN;
#elif SERIAL_INFLUX_VERSION == 1
    if (strlen(SERIAL_INFLUX_DATABASE) == 0) {
        return false;
    }
    writeUrl_ = baseUrl_ + "/write?db=" + urlEncode(String(SERIAL_INFLUX_DATABASE)) + "&precision=ns";
    if (strlen(SERIAL_INFLUX_USERNAME) > 0) {
        writeUrl_ += "&u=";
        writeUrl_ += urlEncode(String(SERIAL_INFLUX_USERNAME));
        writeUrl_ += "&p=";
        writeUrl_ += urlEncode(String(SERIAL_INFLUX_PASSWORD));
    }
#else
#error Unsupported SERIAL_INFLUX_VERSION value
#endif
    return true;
}

String InfluxLineWriter::buildPayload(const String &message) const {
    String payload = measurement_;
    String tags = staticTags_;

    if (useUniqueDeviceTag_ && UniqueDeviceName[0] != '\0') {
        if (!tags.isEmpty()) tags += ',';
        tags += "device=";
        tags += escapeTag(String(UniqueDeviceName));
    }

    if (includeLevel_) {
        String level = detectLevel(message);
        if (!tags.isEmpty()) tags += ',';
        tags += "level=";
        tags += escapeTag(level);
    }

    if (!tags.isEmpty()) {
        payload += ',';
        payload += tags;
    }

    payload += ' ';
    payload += "message=\"";
    payload += escapeField(message);
    payload += "\"";

    return payload;
}

bool InfluxLineWriter::log(const String &message, bool *skipped) {
    if (skipped) {
        *skipped = false;
    }

    if (!enabled_ || message.isEmpty()) {
        if (skipped) {
            *skipped = true;
        }
        return false;
    }
    if (!WiFi.isConnected()) {
#if SERIAL_INFLUX_TRACE
        if (traceSerial_) {
            traceSerial_->println(F("[Influx] Skip write: WiFi disconnected"));
        }
#endif
        if (skipped) {
            *skipped = true;
        }
        return false;
    }

    const String payload = buildPayload(message);

#if SERIAL_INFLUX_TRACE
    uint32_t start = millis();
    if (traceSerial_) {
        traceSerial_->printf("[Influx] POST len=%u\n", payload.length());
    }
#endif

    bool ok = postLine(payload);

#if SERIAL_INFLUX_TRACE
    if (traceSerial_) {
        uint32_t elapsed = millis() - start;
        traceSerial_->printf("[Influx] POST %s (%lu ms)\n", ok ? "ok" : "fail", static_cast<unsigned long>(elapsed));
    }
#endif

    return ok;
}

bool InfluxLineWriter::postLine(const String &payload) {
    HTTPClient http;

    if (secure_) {
        WiFiClientSecure client;
#if SERIAL_INFLUX_VERIFY_TLS
        if (strlen(SERIAL_INFLUX_TLS_FINGERPRINT) > 0) {
            client.setFingerprint(SERIAL_INFLUX_TLS_FINGERPRINT);
        }
#else
        client.setInsecure();
#endif
        if (!http.begin(client, writeUrl_)) {
            return false;
        }
    } else {
        WiFiClient client;
        if (!http.begin(client, writeUrl_)) {
            return false;
        }
    }

    http.setTimeout(SERIAL_INFLUX_HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "text/plain");
    if (!authHeader_.isEmpty()) {
        http.addHeader("Authorization", authHeader_);
    }

    int status = http.POST(payload);
    http.end();
    return status >= 200 && status < 300;
}

SerialInfluxProxy::SerialInfluxProxy(HardwareSerial &base)
    : base_(base) {
    forwardingEnabled_ = writer_.isEnabled();
#if SERIAL_INFLUX_TRACE
    writer_.setTraceSerial(&base_);
#endif
}

void SerialInfluxProxy::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin) {
    base_.begin(baud, config, rxPin, txPin);
}

void SerialInfluxProxy::updateBaudRate(unsigned long baud) {
    base_.updateBaudRate(baud);
}

void SerialInfluxProxy::end() {
    base_.end();
}

int SerialInfluxProxy::available(void) {
    return base_.available();
}

int SerialInfluxProxy::availableForWrite(void) {
    return base_.availableForWrite();
}

int SerialInfluxProxy::peek(void) {
    return base_.peek();
}

int SerialInfluxProxy::read(void) {
    return base_.read();
}

void SerialInfluxProxy::flush(void) {
    base_.flush();
    flushLine();
}

size_t SerialInfluxProxy::write(uint8_t c) {
    size_t written = base_.write(c);
    if (!forwardingEnabled_) {
        return written;
    }
    pushChar(static_cast<char>(c));
    return written;
}

size_t SerialInfluxProxy::write(const uint8_t *buffer, size_t size) {
    size_t written = base_.write(buffer, size);
    if (!forwardingEnabled_ || buffer == nullptr) {
        return written;
    }
    for (size_t i = 0; i < size; ++i) {
        pushChar(static_cast<char>(buffer[i]));
    }
    return written;
}

void SerialInfluxProxy::pushChar(char c) {
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        flushLine();
        return;
    }
    if (lineBuffer_.length() >= SERIAL_INFLUX_MAX_LINE) {
        flushLine();
    }
    lineBuffer_ += c;
}

void SerialInfluxProxy::flushLine() {
    if (lineBuffer_.isEmpty()) {
        return;
    }
    updateForwardingState();
    if (!forwardingEnabled_) {
#if SERIAL_INFLUX_TRACE
        if (!lineBuffer_.isEmpty()) {
            base_.println(F("[Influx] Forwarding disabled, skipping line"));
        }
#endif
        lineBuffer_.clear();
        return;
    }

    bool skipped = false;
    bool ok = writer_.log(lineBuffer_, &skipped);
    if (ok) {
        failureCount_ = 0;
    } else if (!skipped) {
        ++failureCount_;
        if (failureCount_ >= SERIAL_INFLUX_MAX_FAILURES) {
            forwardingEnabled_ = false;
            disabledSinceMs_ = millis();
#if SERIAL_INFLUX_TRACE
            base_.println(F("[Influx] Disabling forwarding after repeated failures"));
#endif
        }
    }
    lineBuffer_.clear();
}

void SerialInfluxProxy::updateForwardingState() {
    if (forwardingEnabled_) {
        return;
    }
#if SERIAL_INFLUX_REENABLE_AFTER_MS
    if (!writer_.isEnabled()) {
        return;
    }
    if (disabledSinceMs_ == 0) {
        return;
    }
    uint32_t elapsed = millis() - disabledSinceMs_;
    if (elapsed >= SERIAL_INFLUX_REENABLE_AFTER_MS) {
        forwardingEnabled_ = true;
        failureCount_ = 0;
        disabledSinceMs_ = 0;
#if SERIAL_INFLUX_TRACE
        base_.println(F("[Influx] Forwarding re-enabled after cooldown"));
#endif
    }
#endif
}

namespace influxserial_detail {

HardwareSerial &hardwareSerial() {
    return ::Serial;
}

SerialInfluxProxy &proxy() {
    static SerialInfluxProxy instance(hardwareSerial());
    return instance;
}

} // namespace influxserial_detail

#endif // ENABLE_INFLUX_SERIAL_LOGGER
