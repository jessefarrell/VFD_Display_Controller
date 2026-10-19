#ifndef SETTINGS_FLASH_H
#define SETTINGS_FLASH_H

#include <cstddef>
#include <cstdint>

constexpr size_t SETTINGS_SECTOR_SIZE = 4096;
constexpr size_t SETTINGS_SLOT_COUNT = 2;
constexpr size_t CLOCK_CHECKPOINT_SECTORS = 8;
constexpr size_t CLOCK_CHECKPOINT_PAGE_SIZE = 256;
constexpr size_t CLOCK_CHECKPOINT_PAGES_PER_SECTOR = SETTINGS_SECTOR_SIZE / CLOCK_CHECKPOINT_PAGE_SIZE;

// Platform boundary; the host tests supply a simulated NOR flash implementation.
const uint8_t* settings_flash_slot(unsigned slot);
bool settings_flash_write(unsigned slot, const uint8_t* data, size_t size);
bool settings_flash_erase(unsigned slot);

// Separate append-only clock journal, before the existing settings sectors.
const uint8_t* clock_flash_sector(unsigned sector);
bool clock_flash_program(unsigned sector, unsigned page, const uint8_t* data);
bool clock_flash_erase(unsigned sector);

#endif
