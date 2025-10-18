#pragma once

#include <Arduino.h>

#ifdef ENABLE_INFLUX_SERIAL_LOGGER

#include <Stream.h>
#include <HardwareSerial.h>

#ifndef SERIAL_INFLUX_URL
#define SERIAL_INFLUX_URL ""
#endif

#ifndef SERIAL_INFLUX_VERSION
#define SERIAL_INFLUX_VERSION 2
#endif

#ifndef SERIAL_INFLUX_BUCKET
#define SERIAL_INFLUX_BUCKET ""
#endif

#ifndef SERIAL_INFLUX_ORG
#define SERIAL_INFLUX_ORG ""
#endif

#ifndef SERIAL_INFLUX_TOKEN
#define SERIAL_INFLUX_TOKEN ""
#endif

#ifndef SERIAL_INFLUX_DATABASE
#define SERIAL_INFLUX_DATABASE ""
#endif

#ifndef SERIAL_INFLUX_USERNAME
#define SERIAL_INFLUX_USERNAME ""
#endif

#ifndef SERIAL_INFLUX_PASSWORD
#define SERIAL_INFLUX_PASSWORD ""
#endif

#ifndef SERIAL_INFLUX_MEASUREMENT
#define SERIAL_INFLUX_MEASUREMENT "serial_log"
#endif

#ifndef SERIAL_INFLUX_DEVICE_TAG
#define SERIAL_INFLUX_DEVICE_TAG ""
#endif

#ifndef SERIAL_INFLUX_SOURCE_TAG
#define SERIAL_INFLUX_SOURCE_TAG "serial"
#endif

#ifndef SERIAL_INFLUX_INCLUDE_LEVEL
#define SERIAL_INFLUX_INCLUDE_LEVEL 1
#endif

#ifndef SERIAL_INFLUX_DEFAULT_LEVEL
#define SERIAL_INFLUX_DEFAULT_LEVEL "info"
#endif

#ifndef SERIAL_INFLUX_HTTP_TIMEOUT_MS
#define SERIAL_INFLUX_HTTP_TIMEOUT_MS 5000
#endif

#ifndef SERIAL_INFLUX_VERIFY_TLS
#define SERIAL_INFLUX_VERIFY_TLS 0
#endif

#ifndef SERIAL_INFLUX_TLS_FINGERPRINT
#define SERIAL_INFLUX_TLS_FINGERPRINT ""
#endif

#ifndef SERIAL_INFLUX_MAX_LINE
#define SERIAL_INFLUX_MAX_LINE 384
#endif

#ifndef SERIAL_INFLUX_TRACE
#define SERIAL_INFLUX_TRACE 0
#endif

#ifndef SERIAL_INFLUX_MAX_FAILURES
#define SERIAL_INFLUX_MAX_FAILURES 3
#endif

#ifndef SERIAL_INFLUX_REENABLE_AFTER_MS
#define SERIAL_INFLUX_REENABLE_AFTER_MS 60000UL
#endif

class InfluxLineWriter {
public:
    InfluxLineWriter();

    bool isEnabled() const { return enabled_; }

    bool log(const String &message, bool *skipped = nullptr);

#if SERIAL_INFLUX_TRACE
    void setTraceSerial(HardwareSerial *serial) { traceSerial_ = serial; }
#endif

private:
    bool buildWriteUrl();
    String buildPayload(const String &message) const;
    bool postLine(const String &payload);

    String baseUrl_;
    String writeUrl_;
    String measurement_;
    String staticTags_;
    String authHeader_;
    bool enabled_ = false;
    bool includeLevel_ = SERIAL_INFLUX_INCLUDE_LEVEL != 0;
    bool secure_ = false;
    bool useUniqueDeviceTag_ = false;
#if SERIAL_INFLUX_TRACE
    HardwareSerial *traceSerial_ = nullptr;
#endif
};

class SerialInfluxProxy : public Stream {
public:
    explicit SerialInfluxProxy(HardwareSerial &base);

    void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1);
    void updateBaudRate(unsigned long baud);
    void end();

    int available(void) override;
    int availableForWrite(void);
    int peek(void) override;
    int read(void) override;
    void flush(void) override;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

private:
    void pushChar(char c);
    void flushLine();
    void updateForwardingState();

    HardwareSerial &base_;
    InfluxLineWriter writer_;
    String lineBuffer_;
    bool forwardingEnabled_ = false;
    uint8_t failureCount_ = 0;
    uint32_t disabledSinceMs_ = 0;
};

namespace influxserial_detail {
    SerialInfluxProxy &proxy();
}

#ifndef INFLUX_SERIAL_IMPLEMENTATION
#ifdef Serial
#undef Serial
#endif
#define Serial influxserial_detail::proxy()
#endif

#endif // ENABLE_INFLUX_SERIAL_LOGGER
