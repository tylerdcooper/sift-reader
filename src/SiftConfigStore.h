#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

/**
 * Singleton store for the Sift server base URL and per-user feed token,
 * persisted to the SD card at /.crosspoint/sift.json. Set via serial
 * provisioning (CMD:SIFT) at flash time, or from the Sift screen.
 *
 * The token is XOR-obfuscated with the device's hardware MAC and base64-encoded
 * before writing (same scheme as the other credential stores) — not
 * cryptographically secure, but keeps it out of casual reads and ties it to
 * this device.
 */
class SiftConfigStore : public PersistableStore<SiftConfigStore> {
 private:
  std::string baseUrl;  // e.g. "https://sift.example.com" (no trailing slash)
  std::string token;    // "<userId>_<feedToken>" — grants read access to the user's feeds

  SiftConfigStore() = default;
  ~SiftConfigStore() = default;

  friend class PersistableStore<SiftConfigStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/sift.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Set + persist both values at once (the provisioning entry point).
  void setConfig(const std::string& url, const std::string& tok);
  void clear();

  const std::string& getToken() const { return token; }
  bool hasConfig() const { return !baseUrl.empty() && !token.empty(); }

  // Base URL with any trailing slashes stripped.
  std::string getBaseUrl() const;
  // Full OPDS acquisition-feed URL for a collection type ("saved"/"favorites"/"all").
  std::string getOpdsUrl(const char* type) const;
};

#define SIFT_CONFIG SiftConfigStore::getInstance()
