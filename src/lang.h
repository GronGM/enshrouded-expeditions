// Russian and English: every text the player sees is written as L("по-русски", "in English").
#pragma once
#include <cstdint>
#include <string>

namespace exped::lang {

enum class Lang { Ru, En };

Lang current();
// "auto" (the system's language), "ru" or "en"
void set(const std::string& setting);
inline bool ru() { return current() == Lang::Ru; }
inline const char* L(const char* ru_text, const char* en_text) { return ru() ? ru_text : en_text; }

// Names by the game's ids, in the current language; `configured` is the settings' name,
// shown instead when the player wrote their own.
std::string trade(uint32_t workshop, const std::string& configured);
std::string region(int area, const std::string& configured);
// What a trade brings home ("" for one we do not know).
std::string brings(uint32_t workshop);
// "hurt", "cache", "rich", "weather" -> the line for a report; also reads the Russian
// texts older versions saved.
std::string event_text(const std::string& id);
std::string event_id(const std::string& saved);

std::string items_word(uint32_t n);   // предмет / предмета / предметов, item / items
std::string routes_word(size_t n);    // маршрут / маршрута / маршрутов, route / routes
std::string minutes(double seconds);  // "1 ч 30 мин", "1 h 30 min"

}  // namespace exped::lang
