#include "settings_flash.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"

// Defined by settings.ld; these sectors are outside the firmware image.
extern "C" uint8_t __settings_start[];
extern "C" uint8_t __clock_checkpoint_start[];

static_assert(SETTINGS_SECTOR_SIZE == FLASH_SECTOR_SIZE);
static_assert(CLOCK_CHECKPOINT_PAGE_SIZE == FLASH_PAGE_SIZE);

namespace {
struct WriteOperation {
    uint32_t offset;
    const uint8_t* data;
    size_t size;
    bool erase;
};

void write_record(void* context) {
    auto& op = *static_cast<WriteOperation*>(context);
    // The SDK restores XIP before returning from each call. The source
    // buffer and context are in RAM; flash_safe_execute disables IRQs.
    if (op.erase) flash_range_erase(op.offset, FLASH_SECTOR_SIZE);
    if (op.size > 0) flash_range_program(op.offset, op.data, op.size);
}
}

const uint8_t* settings_flash_slot(unsigned slot) {
    return __settings_start + slot * SETTINGS_SECTOR_SIZE;
}

bool settings_flash_write(unsigned slot, const uint8_t* data, size_t size) {
    if (slot >= SETTINGS_SLOT_COUNT || size == 0 || size > SETTINGS_SECTOR_SIZE ||
        size % FLASH_PAGE_SIZE != 0) return false;
    WriteOperation op{
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(settings_flash_slot(slot)) - XIP_BASE),
        data, size, true
    };
    // This firmware runs on core 0 only. If core 1 is added, initialize
    // the SDK's multicore flash lockout before allowing settings writes.
    return flash_safe_execute(write_record, &op, 1000) == PICO_OK;
}

bool settings_flash_erase(unsigned slot) {
    if (slot >= SETTINGS_SLOT_COUNT) return false;
    WriteOperation op{
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(settings_flash_slot(slot)) - XIP_BASE),
        nullptr, 0, true
    };
    return flash_safe_execute(write_record, &op, 1000) == PICO_OK;
}

const uint8_t* clock_flash_sector(unsigned sector) {
    return __clock_checkpoint_start + sector * SETTINGS_SECTOR_SIZE;
}

bool clock_flash_program(unsigned sector, unsigned page, const uint8_t* data) {
    if (sector >= CLOCK_CHECKPOINT_SECTORS || page >= CLOCK_CHECKPOINT_PAGES_PER_SECTOR) return false;
    WriteOperation op{
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(clock_flash_sector(sector)) - XIP_BASE +
                              page * CLOCK_CHECKPOINT_PAGE_SIZE),
        data, CLOCK_CHECKPOINT_PAGE_SIZE, false
    };
    return flash_safe_execute(write_record, &op, 1000) == PICO_OK;
}

bool clock_flash_erase(unsigned sector) {
    if (sector >= CLOCK_CHECKPOINT_SECTORS) return false;
    WriteOperation op{
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(clock_flash_sector(sector)) - XIP_BASE),
        nullptr, 0, true
    };
    return flash_safe_execute(write_record, &op, 1000) == PICO_OK;
}
