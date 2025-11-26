// Example usage of flash_to_blackbox() on STM32H7
// This is a *pattern*, not a drop-in main.c replacement.
// Assumes you have:
//   - QSPI + hqspi set up by CubeMX
//   - FreeRTOS task or background loop to run blackbox_flush_task()

#include "blackbox_flash.hpp"

constexpr std::size_t LOG_ENTRY_SIZE      = 256;
constexpr std::size_t LOG_BUFFER_ENTRIES  = 64;

// Simple RAM ring buffer for log entries
static uint8_t  log_buffer[LOG_BUFFER_ENTRIES][LOG_ENTRY_SIZE];
static uint32_t write_idx  = 0;  // producer (1 kHz loop)
static uint32_t commit_idx = 0;  // consumer (flush task)

// -----------------------------------------------------------------------------
// 1 ms loop: fill RAM buffer only (never touches flash)
// -----------------------------------------------------------------------------
void loop_1ms()
{
    uint32_t idx = write_idx;

    // TODO: encode your data into log_buffer[idx]
    // Example content: simple counter/time marker
    for (std::size_t i = 0; i < LOG_ENTRY_SIZE; ++i) {
        log_buffer[idx][i] = static_cast<uint8_t>((idx + i) & 0xFF);
    }

    // Advance write index (wrap in RAM ring)
    write_idx = (idx + 1) % LOG_BUFFER_ENTRIES;
}

// -----------------------------------------------------------------------------
// Background task: drains RAM buffer to QSPI blackbox
// Call this from a low-priority task or main background loop.
// -----------------------------------------------------------------------------
void blackbox_flush_task()
{
    uint32_t local_commit = commit_idx;

    while (local_commit != write_idx) {
        flash_to_blackbox(log_buffer[local_commit], LOG_ENTRY_SIZE);

        local_commit = (local_commit + 1) % LOG_BUFFER_ENTRIES;
        commit_idx   = local_commit;
    }
}

// -----------------------------------------------------------------------------
// Sketch of integration
// -----------------------------------------------------------------------------
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_QSPI_Init();   // CubeMX-generated, sets up hqspi

    // Create a 1 ms timer / interrupt that calls loop_1ms()
    // For baremetal, call loop_1ms() from SysTick_Handler or a TIM ISR.

    while (1) {
        blackbox_flush_task();   // run in background
        // other low-priority work...
    }
}
*/
