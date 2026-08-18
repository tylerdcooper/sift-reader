#include "SiftConfigStore.h"

#include <Logging.h>
#include <ObfuscationUtils.h>

void SiftConfigStore::toJson(JsonDocument& doc) const {
  doc["baseUrl"] = baseUrl;
  doc["token_obf"] = obfuscation::obfuscateToBase64(token);
}

bool SiftConfigStore::fromJson(JsonVariantConst doc) {
  baseUrl = doc["baseUrl"] | "";
  const char* obf = doc["token_obf"] | "";
  token = obf[0] ? obfuscation::deobfuscateFromBase64(obf) : std::string();
  return true;
}

void SiftConfigStore::setConfig(const std::string& url, const std::string& tok) {
  baseUrl = url;
  token = tok;
  saveToFile();
  LOG_DBG("SIFT", "Config set: base=%s token=%s", url.c_str(), tok.empty() ? "(none)" : "(set)");
}

void SiftConfigStore::clear() {
  baseUrl.clear();
  token.clear();
  saveToFile();
  LOG_DBG("SIFT", "Config cleared");
}

std::string SiftConfigStore::getBaseUrl() const {
  std::string url = baseUrl;
  while (!url.empty() && url.back() == '/') {
    url.pop_back();
  }
  return url;
}

std::string SiftConfigStore::getOpdsUrl(const char* type) const {
  // Token characters ("<int>_<hex>") are all URL-safe, so no encoding needed.
  std::string url = getBaseUrl();
  url += "/api/opds/";
  url += type;
  url += "?token=";
  url += token;
  return url;
}
