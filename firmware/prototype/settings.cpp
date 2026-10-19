#include "settings.h"
#include "settings_flash.h"
#include <cstdio>
#include <cstring>

namespace {
// Explicit little-endian layout, independent of C++ padding. Version 3
// adds static text; version 1 and 2 records remain readable for upgrades.
constexpr uint32_t MAGIC = 0x53444656; // "VFDS"
constexpr uint16_t VERSION = 3;
constexpr size_t TEXT_OFFSET = 35;
constexpr size_t STATIC_TEXT_OFFSET = TEXT_OFFSET + MARQUEE_TEXT_MAX_LEN + 1;
constexpr size_t RECORD_SIZE = ((STATIC_TEXT_OFFSET + STATIC_TEXT_MAX_LEN + 1 + 255) / 256) * 256;
constexpr size_t LEGACY_RECORD_SIZE = 1280;
constexpr size_t LEGACY_TEXT_SIZE = 1001;
constexpr size_t VERSION_2_RECORD_SIZE = 2304;
static_assert(STATIC_TEXT_OFFSET + STATIC_TEXT_MAX_LEN + 1 <= RECORD_SIZE);
static_assert(RECORD_SIZE <= SETTINGS_SECTOR_SIZE);

constexpr AppSettings defaults{};
AppSettings current;
// These operations run on core 0 only. Keep large working buffers out of
// the Pico's 2 KiB stack, including while calling the USB and flash SDKs.
AppSettings candidate;
alignas(4) uint8_t record_buffer[RECORD_SIZE];
int active_slot = -1;
uint32_t sequence = 0;
constexpr uint32_t CLOCK_MAGIC = 0x50434456; // "VDCP"
constexpr size_t CLOCK_PAGES = CLOCK_CHECKPOINT_SECTORS * CLOCK_CHECKPOINT_PAGES_PER_SECTOR;

void put16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t* p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint16_t get16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (uint16_t(p[1]) << 8));
}
uint32_t get32(const uint8_t* p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
    return v;
}

uint32_t checksum(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; ++i) {
        // The checksum field itself (bytes 12..15) is treated as zero.
        crc ^= (i >= 12 && i < 16) ? 0 : data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool printable_text(const char* text, size_t capacity) {
    for (size_t i = 0; i < capacity; ++i) {
        unsigned char c = text[i];
        if (c == 0) return true;
        if (c < 0x20 || c > 0x7e) return false;
    }
    return false;
}

bool valid_date(int year, unsigned month, unsigned day, unsigned hour, unsigned minute, unsigned second) {
    if (year < 1 || year > 4095 || month < 1 || month > 12 || day < 1 ||
        hour > 23 || minute > 59 || second > 59) return false;
    constexpr uint8_t month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned max_day = month_days[month - 1];
    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ++max_day;
    return day <= max_day;
}

bool valid(const AppSettings& s) {
    if (static_cast<uint8_t>(s.startup_app) > static_cast<uint8_t>(AppId::STATIC_TEXT) ||
        s.animation_effect > ANIMATION_EFFECT_COUNT || s.shuffle_ms == 0 ||
        s.marquee_rate_ms == 0 || s.marquee_rate_ms > MAX_MARQUEE_RATE_MS ||
        !valid_date(s.year, s.month, s.day, s.hour, s.minute, s.second)) return false;
    return printable_text(s.marquee_text, sizeof(s.marquee_text)) &&
           printable_text(s.static_text, sizeof(s.static_text));
}

void encode(uint8_t* data, const AppSettings& s, uint32_t seq) {
    std::memset(data, 0, RECORD_SIZE);
    put32(data, MAGIC);
    put16(data + 4, VERSION);
    put16(data + 6, RECORD_SIZE);
    put32(data + 8, seq);
    data[16] = static_cast<uint8_t>(s.startup_app);
    data[17] = s.animation_effect;
    data[18] = s.cursor_blink;
    data[19] = s.clock_time_set;
    put32(data + 20, s.shuffle_ms);
    put32(data + 24, s.marquee_rate_ms);
    put16(data + 28, s.year);
    data[30] = s.month;
    data[31] = s.day;
    data[32] = s.hour;
    data[33] = s.minute;
    data[34] = s.second;
    // Canonicalize bytes after the terminator, so shorter messages don't
    // trigger writes merely because of unused bytes left in a RAM buffer.
    std::memcpy(data + TEXT_OFFSET, s.marquee_text, std::strlen(s.marquee_text));
    std::memcpy(data + STATIC_TEXT_OFFSET, s.static_text, std::strlen(s.static_text));
    put32(data + 12, checksum(data, RECORD_SIZE));
}

bool decode(const uint8_t* data, AppSettings& s) {
    uint16_t version = get16(data + 4);
    size_t size = get16(data + 6);
    bool legacy = version == 1 && size == LEGACY_RECORD_SIZE;
    bool version_2 = version == 2 && size == VERSION_2_RECORD_SIZE;
    if (get32(data) != MAGIC || (!legacy && !version_2 && (version != VERSION || size != RECORD_SIZE)) ||
        get32(data + 12) != checksum(data, size) ||
        data[18] > 1 || data[19] > 1) return false;
    s.startup_app = static_cast<AppId>(data[16]);
    s.animation_effect = data[17];
    s.cursor_blink = data[18] != 0;
    s.clock_time_set = data[19] != 0;
    s.shuffle_ms = get32(data + 20);
    s.marquee_rate_ms = get32(data + 24);
    // Recover a previously saved excessive delay without losing other settings.
    if (s.marquee_rate_ms > MAX_MARQUEE_RATE_MS) s.marquee_rate_ms = DEFAULT_MARQUEE_RATE_MS;
    s.year = get16(data + 28);
    s.month = data[30];
    s.day = data[31];
    s.hour = data[32];
    s.minute = data[33];
    s.second = data[34];
    std::memset(s.marquee_text, 0, sizeof(s.marquee_text));
    std::memcpy(s.marquee_text, data + TEXT_OFFSET, legacy ? LEGACY_TEXT_SIZE : sizeof(s.marquee_text));
    // A legacy record must contain its terminator within its original capacity.
    if (legacy && !std::memchr(s.marquee_text, 0, LEGACY_TEXT_SIZE)) return false;
    if (version == VERSION) {
        std::memcpy(s.static_text, data + STATIC_TEXT_OFFSET, sizeof(s.static_text));
    } else {
        std::memcpy(s.static_text, defaults.static_text, sizeof(s.static_text));
    }
    return valid(s);
}

bool newer(uint32_t a, uint32_t b) {
    uint32_t distance = a - b;
    return distance != 0 && distance < 0x80000000u;
}

const uint8_t* clock_page(unsigned page) {
    return clock_flash_sector(page / CLOCK_CHECKPOINT_PAGES_PER_SECTOR) +
           (page % CLOCK_CHECKPOINT_PAGES_PER_SECTOR) * CLOCK_CHECKPOINT_PAGE_SIZE;
}

bool erased(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) if (data[i] != 0xff) return false;
    return true;
}

struct JournalHead { int page = -1; uint32_t sequence = 0; };

JournalHead scan_clock_journal(bool restore) {
    JournalHead head;
    int matching_page = -1;
    uint32_t matching_sequence = 0;
    for (unsigned page = 0; page < CLOCK_PAGES; ++page) {
        const uint8_t* data = clock_page(page);
        if (get32(data) != CLOCK_MAGIC || get16(data + 4) != 1 ||
            get16(data + 6) != CLOCK_CHECKPOINT_PAGE_SIZE ||
            get32(data + 12) != checksum(data, CLOCK_CHECKPOINT_PAGE_SIZE) ||
            !valid_date(get16(data + 20), data[22], data[23], data[24], data[25], data[26])) continue;
        uint32_t seq = get32(data + 8);
        if (head.page < 0 || newer(seq, head.sequence)) head = {int(page), seq};
        if (restore && current.clock_time_set && get32(data + 16) == sequence &&
            (matching_page < 0 || newer(seq, matching_sequence))) {
            matching_page = page;
            matching_sequence = seq;
        }
    }
    if (matching_page >= 0) {
        const uint8_t* data = clock_page(matching_page);
        current.year = get16(data + 20);
        current.month = data[22];
        current.day = data[23];
        current.hour = data[24];
        current.minute = data[25];
        current.second = data[26];
    }
    return head;
}

bool clear_clock_journal() {
    for (unsigned sector = 0; sector < CLOCK_CHECKPOINT_SECTORS; ++sector) {
        const uint8_t* data = clock_flash_sector(sector);
        if (erased(data, SETTINGS_SECTOR_SIZE)) continue;
        if (!clock_flash_erase(sector) || !erased(data, SETTINGS_SECTOR_SIZE)) {
            std::printf("-> Defaults saved, but clock checkpoint cleanup failed\n");
            return false;
        }
    }
    return true;
}
} // namespace

AppSettings& settings() { return current; }

bool settings_load() {
    // Keep the defaults in flash instead of constructing another large
    // temporary on the Pico's small stack (including in Debug builds).
    current = defaults;
    active_slot = -1;
    sequence = 0;
    for (unsigned slot = 0; slot < SETTINGS_SLOT_COUNT; ++slot) {
        const uint8_t* data = settings_flash_slot(slot);
        if (!decode(data, candidate)) continue;
        uint32_t candidate_sequence = get32(data + 8);
        // Modular comparison also handles the sequence counter wrapping.
        uint32_t distance = candidate_sequence - sequence;
        if (active_slot < 0 || (distance != 0 && distance < 0x80000000u)) {
            current = candidate;
            sequence = candidate_sequence;
            active_slot = slot;
        }
    }
    if (active_slot >= 0) scan_clock_journal(true);
    return active_slot >= 0;
}

bool settings_save(bool force) {
    if (!valid(current)) {
        std::printf("-> Invalid settings; changes could not be saved\n");
        return false;
    }
    uint8_t* data = record_buffer;
    encode(data, current, sequence);
    if (!force && active_slot >= 0 &&
        std::memcmp(data, settings_flash_slot(active_slot), RECORD_SIZE) == 0) return true;

    unsigned target = (active_slot < 0) ? 0 : (unsigned(active_slot) ^ 1u);
    encode(data, current, sequence + 1);
    // Always erase/program the other sector, preserving the last valid copy.
    if (!settings_flash_write(target, data, RECORD_SIZE) ||
        std::memcmp(data, settings_flash_slot(target), RECORD_SIZE) != 0) {
        std::printf("-> Settings save failed; changes apply until reboot only\n");
        return false;
    }
    active_slot = target;
    ++sequence;
    return true;
}

bool settings_checkpoint_clock() {
    if (active_slot < 0 || !current.clock_time_set || !valid(current) || settings_flash_slot(active_slot)[19] != 1) {
        std::printf("-> Clock checkpoint requires a valid saved clock configuration\n");
        return false;
    }
    // Re-scan even after failures: a write may have completed before a power
    // interruption/error, while a torn page must never be programmed again.
    JournalHead head = scan_clock_journal(false);
    unsigned page = head.page < 0 ? 0 : (unsigned(head.page) + 1) % CLOCK_PAGES;
    for (size_t checked = 0; checked < CLOCK_PAGES; ++checked, page = (page + 1) % CLOCK_PAGES) {
        unsigned sector = page / CLOCK_CHECKPOINT_PAGES_PER_SECTOR;
        unsigned in_sector = page % CLOCK_CHECKPOINT_PAGES_PER_SECTOR;
        if (!erased(clock_page(page), CLOCK_CHECKPOINT_PAGE_SIZE)) {
            if (in_sector != 0 || (head.page >= 0 && sector == unsigned(head.page) / CLOCK_CHECKPOINT_PAGES_PER_SECTOR))
                continue;
            if (!clock_flash_erase(sector) || !erased(clock_flash_sector(sector), SETTINGS_SECTOR_SIZE)) {
                std::printf("-> Clock checkpoint sector erase failed\n");
                return false;
            }
        }
        alignas(4) uint8_t data[CLOCK_CHECKPOINT_PAGE_SIZE] = {};
        put32(data, CLOCK_MAGIC);
        put16(data + 4, 1);
        put16(data + 6, CLOCK_CHECKPOINT_PAGE_SIZE);
        put32(data + 8, head.sequence + 1);
        put32(data + 16, sequence); // checkpoints belong to this settings revision
        put16(data + 20, current.year);
        data[22] = current.month;
        data[23] = current.day;
        data[24] = current.hour;
        data[25] = current.minute;
        data[26] = current.second;
        put32(data + 12, checksum(data, sizeof(data)));
        if (!clock_flash_program(sector, in_sector, data) ||
            std::memcmp(data, clock_page(page), sizeof(data)) != 0) {
            std::printf("-> Clock checkpoint save failed; the previous saved time remains available\n");
            return false;
        }
        return true;
    }
    std::printf("-> No writable clock checkpoint page available\n");
    return false;
}

bool settings_factory_reset() {
    current = defaults;
    // Commit defaults first so a power loss during backup cleanup cannot
    // resurrect the previous configuration as the newest valid record.
    if (!settings_save()) return false;
    unsigned previous_slot = unsigned(active_slot) ^ 1u;
    const uint8_t* previous = settings_flash_slot(previous_slot);
    bool erased = true;
    for (size_t i = 0; i < SETTINGS_SECTOR_SIZE; ++i) erased &= previous[i] == 0xff;
    if (erased) return clear_clock_journal();
    if (!settings_flash_erase(previous_slot)) {
        std::printf("-> Defaults saved, but the previous settings copy could not be erased\n");
        return false;
    }
    for (size_t i = 0; i < SETTINGS_SECTOR_SIZE; ++i) {
        if (previous[i] != 0xff) {
            std::printf("-> Defaults saved, but previous settings erase verification failed\n");
            return false;
        }
    }
    return clear_clock_journal();
}
