#ifndef SETTINGS_H
#define SETTINGS_H

#include <cstddef>
#include <cstdint>

enum class AppId : uint8_t { MENU = 0, CLOCK = 1, DISPLAY_TEST = 2, ANIMATION = 3, STATIC_TEXT = 4 };

constexpr size_t MARQUEE_TEXT_MAX_LEN = 2048;
constexpr uint32_t MAX_MARQUEE_RATE_MS = 10000;
constexpr uint32_t DEFAULT_MARQUEE_RATE_MS = 150;
constexpr uint8_t ANIMATION_EFFECT_COUNT = 6;
constexpr size_t STATIC_TEXT_MAX_LEN = 20;

struct AppSettings {
    AppId startup_app = AppId::MENU;
    // Matches the animation menu: 0 = Random, 1..6 = individual effects.
    uint8_t animation_effect = 0;
    bool cursor_blink = false;
    bool clock_time_set = false;
    uint32_t shuffle_ms = 8000;
    uint32_t marquee_rate_ms = DEFAULT_MARQUEE_RATE_MS;
    int16_t year = 2026;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    char marquee_text[MARQUEE_TEXT_MAX_LEN + 1] = "  Schneider Electric  ";
    char static_text[STATIC_TEXT_MAX_LEN + 1] = "VFD READY";
};

// Call once at startup. Invalid, erased, or unsupported records use defaults.
bool settings_load();
AppSettings& settings();
// Saves changes; force starts a new revision even for identical values so an
// explicit clock adjustment cannot be overridden by an older checkpoint.
// Failure leaves RAM settings applied and reports on USB.
bool settings_save(bool force = false);
// Appends current date/time to the clock journal without rewriting settings.
// Requires a configured clock and a successfully saved configuration.
bool settings_checkpoint_clock();
// Applies defaults, commits them, then erases the previous copy and clock journal.
// On failure defaults remain active, but the persistent reset may be incomplete.
bool settings_factory_reset();

#endif
