#pragma once
#include <ArduinoJson.h>
#include <string.h>
#include "Model.h"

inline bool parseQuote(JsonVariantConst doc, Quote &out, uint32_t now) {
  if (!doc["chart"]["error"].isNull()) return false;
  const auto result = doc["chart"]["result"][0];
  const auto meta = result["meta"];
  if (strcmp(meta["symbol"] | "", "MXN=X") || strcmp(meta["currency"] | "", "MXN")) return false;
  Quote q;
  q.price = meta["regularMarketPrice"] | 0.0f;
  q.marketTime = meta["regularMarketTime"] | uint32_t(0);
  q.fetchedAt = now;
  if (!validPrice(q.price) || q.marketTime < 1700000000 || q.marketTime > now + 300) return false;
  auto times = result["timestamp"].as<JsonArrayConst>();
  auto closes = result["indicators"]["quote"][0]["close"].as<JsonArrayConst>();
  if (times.size() != closes.size() || times.size() < 2 || times.size() >= MAX_POINTS) return false;
  for (size_t i=0; i<times.size(); ++i) {
    if (closes[i].isNull() || !closes[i].is<float>() || !times[i].is<uint32_t>()) continue;
    float v = closes[i].as<float>();
    uint32_t t = times[i].as<uint32_t>();
    if (!validPrice(v) || t < 1700000000 || t > q.marketTime) continue;
    if (q.count && t <= q.points[q.count-1].time) return false;
    q.points[q.count++] = {t, v};
  }
  // End the graph at the actual quote, including the current incomplete hour.
  if (q.count && q.points[q.count-1].time == q.marketTime) q.points[q.count-1].value=q.price;
  else if (q.count < MAX_POINTS) q.points[q.count++]={q.marketTime,q.price};
  if (!validQuote(q)) return false;
  out=q;
  return true;
}
