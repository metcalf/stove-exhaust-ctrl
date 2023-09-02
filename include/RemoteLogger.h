#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

#include <stdint.h>

#define REMOTE_LOG_MESSAGE_LEN 480
#define REMOTE_LOG_IP_TTL_MS 60 * 60 * 1000 // 1 hour TTL

enum LogLevel {
  LEVEL_EMERGENCY = 0,
  LEVEL_ALERT,
  LEVEL_CRITICAL,
  LEVEL_ERROR,
  LEVEL_WARNING,
  LEVEL_NOTICE,
  LEVEL_INFO,
  LEVEL_DEBUG,
};

class RemoteLogger {
public:
  explicit RemoteLogger(const char *my_hostname, const char *dest_host, uint16_t dest_port = 514)
      : dest_port_(dest_port) {
    assert(strnlen(my_hostname, sizeof(my_hostname_)) < sizeof(my_hostname_));
    strncpy(my_hostname_, my_hostname, sizeof(my_hostname_) - 1);

    assert(strnlen(dest_host, sizeof(dest_host_)) < sizeof(dest_host_));
    strncpy(dest_host_, dest_host, sizeof(dest_host_) - 1);
  }

  void log(const char *msg, LogLevel level);

protected:
private:
  char my_hostname_[32];
  char dest_host_[128];
  uint16_t dest_port_;
  bool begun_;
  unsigned long last_ip_resolved_millis_ = 0;
  char buf_[REMOTE_LOG_MESSAGE_LEN];

  WiFiUDP udp_;
  IPAddress dest_ip_;

  bool maybeResolve();
  int send(const char *, size_t, IPAddress, uint16_t);
};
