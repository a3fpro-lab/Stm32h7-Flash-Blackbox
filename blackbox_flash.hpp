#pragma once

// flash_to_blackbox — Eternal Soul Storage (QSPI NOR Flash)
// STM32H7 + Micron/Winbond 128–512 Mbit QSPI NOR
// Intended for STM32H7 HAL QSPI, called from a background logger task.

#include "stm32h7xx_hal.h"
#include <cstddef>
#include <cstdint>

extern QSPI_HandleTypeDef hqspi; // Provided by STM32CubeMX-generated code

// -----------------------------------------------------------------------------
// Blackbox region configuration
// -----------------------------------------------------------------------------
#ifndef BLACKBOX_START_ADDR
#define BLACKBOX_START_ADDR  (0x90000000UL)  // Example: QSPI mapped at 0x9000_0000
#endif

#ifndef BLACKBOX_SIZE
#define BLACKBOX_SIZE        (0x01000000UL)  // Example: 16 MiB reserved region
#endif

// -----------------------------------------------------------------------------
// flash_to_blackbox
// -----------------------------------------------------------------------------
//
// Writes `len` bytes from `data` into a circular region in QSPI NOR flash.
//
// Assumes:
//   - Page size       = 256 bytes
//   - Sector erase    = 4 KB
//   - Page Program    = 0x32 (Quad Input Page Program)
//   - Sector Erase    = 0x20
//
// Usage pattern:
//   - 1 kHz loop writes into a RAM ring buffer (non-blocking).
//   - Background task drains that buffer and calls flash_to_blackbox().
//
// NOTE: `flash_ptr` is static here. If you want it to truly survive reset, place
// it in backup SRAM via linker script / section attributes.
//
[[gnu::always_inline]] inline
void flash_to_blackbox(const uint8_t* data, std::size_t len) noexcept
{
    static uint32_t flash_ptr = BLACKBOX_START_ADDR; // optionally mapped to backup SRAM

    // 1. Page-aligned base (256-byte pages)
    uint32_t aligned_addr = flash_ptr & ~0xFFUL;

    // 2. If we're not at the start of a page, assume we are entering a new page
    //    and erase its 4 KB sector first (just-in-time erase).
    if (flash_ptr != aligned_addr) {
        QSPI_CommandTypeDef cmd = {};
        cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
        cmd.Instruction     = 0x20;                 // Sector Erase (4 KB)
        cmd.AddressMode     = QSPI_ADDRESS_1_LINE;
        cmd.Address         = aligned_addr;
        cmd.DataMode        = QSPI_DATA_NONE;

        (void)HAL_QSPI_Command(&hqspi, &cmd, HAL_MAX_DELAY);

        // Poll until erase complete
        while (__HAL_QSPI_GET_FLAG(&hqspi, QSPI_FLAG_BUSY)) {
            // Optionally add timeout here
        }
    }

    // 3. Quad Input Page Program — up to 256 bytes
    QSPI_CommandTypeDef cmd = {};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction       = 0x32;                    // Quad Page Program
    cmd.AddressMode       = QSPI_ADDRESS_1_LINE;
    cmd.Address           = flash_ptr;
    cmd.DataMode          = QSPI_DATA_4_LINES;
    cmd.NbData            = static_cast<uint32_t>(len);

    (void)HAL_QSPI_Command(&hqspi, &cmd, HAL_MAX_DELAY);

    // HAL_QSPI_Transmit takes non-const pointer; safe const_cast here.
    (void)HAL_QSPI_Transmit(&hqspi,
                            const_cast<uint8_t*>(data),
                            HAL_MAX_DELAY);

    // 4. Advance pointer — circular buffer within [START, START + SIZE)
    flash_ptr += static_cast<uint32_t>(len);
    if (flash_ptr >= BLACKBOX_START_ADDR + BLACKBOX_SIZE) {
        flash_ptr = BLACKBOX_START_ADDR;  // wrap around
    }
}
