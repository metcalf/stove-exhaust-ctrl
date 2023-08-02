#include "RemoteLogger.h"

void RemoteLogger::maybeResolve() {
  unsigned long now = millis();
  if ((now - last_ip_resolved_millis_) < REMOTE_LOG_IP_TTL_MS) {
    // Last entry is still considered valid
    return;
  }

  if(WiFi.hostByName(dest_host_, dest_ip_) == 1) {
    last_ip_resolved_millis_ = millis();
  }
}

int RemoteLogger::send(const char *buffer, size_t buffer_size, IPAddress destination,
                           uint16_t port) {
  size_t written = 0;
  // Note that beginPacket/endPacket treat `1` as success so we invert it on return to follow normal conventions
  if(udp_.beginPacket(dest_ip_, dest_port_) == 1) {
    written = udp_.write((const uint8_t*)buffer, buffer_size);
    if(written == buffer_size && udp_.endPacket() == 1) {
      Serial.printf("Wrote %d/%d bytes of log to %s:%d\n", written, buffer_size,
                  dest_ip_.toString().c_str(), dest_port_);
      return 0;
    }
  }

  // If we failed somewhere, call `stop` to clean up the context
  udp_.stop();
  Serial.printf("Error writing %d bytes to %s:%d: %d\n", buffer_size, dest_ip_.toString().c_str(),
                dest_port_, written);

  return 1;
}

void RemoteLogger::log(const char *msg, LogLevel level) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  int written =
      snprintf(buf_, sizeof(buf_) - 1, "<%u>%s app: %s", 8 + level, my_hostname_, msg);

  // Ensure newline termination for syslog. We only allow writing `sizeof(buf_) - 1` to ensure
  // this doesn't exceed the buffer length
  if (buf_[written - 1] != '\n') {
    buf_[written] = '\n';
    buf_[written + 1] = '\0';
    written++;
  }

  this->maybeResolve();
  if (send(buf_, written, dest_ip_, dest_port_) != 0 && WiFi.status() == WL_CONNECTED) {
    // If the initial `send` fails and wifi is connected, try again
    // If the retry fails, close the socket so we'll re-open on the first attempt next time
    send(buf_, written, dest_ip_, dest_port_);
  }
}
