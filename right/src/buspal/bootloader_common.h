#ifndef __BOOTLOADER_COMMON_H__
#define __BOOTLOADER_COMMON_H__

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include "fsl_common.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

// The following macros are to be used when trying to save code size for specific peripheral configurations
// that will only be using one peripheral instance. most of the peripheral driver code can use multiple instances but by
// just using one
// we can save space
#define USE_ONLY_UART(instance) (defined(BL_FEATURE_UART_OPTIMIZE) && (BL_UART_USED_INSTANCE == instance))
#define USE_ONLY_SPI(instance) (defined(BL_FEATURE_SPI_OPTIMIZE) && (BL_SPI_USED_INSTANCE == instance))
#define USE_ONLY_I2C(instance) (defined(BL_FEATURE_I2C_OPTIMIZE) && (BL_I2C_USED_INSTANCE == instance))

//! @brief Computes the number of elements in an array.
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

//! @name Byte swap macros
//@{
#define BSWAP_16(x) (uint16_t)((((x)&0xFF00) >> 0x8) | (((x)&0xFF) << 0x8))
#define BSWAP_32(val) \
    (uint32_t)((BSWAP_16((uint32_t)(val) & (uint32_t)0xFFFF) << 0x10) | (BSWAP_16((uint32_t)((val) >> 0x10))))
//@}

//! @name Alignment macros
//@{
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, a) ((x) & -(a))
#endif
#ifndef ALIGN_UP
#define ALIGN_UP(x, a) (-(-(x) & -(a)))
#endif
//@}

//! @brief Build a 32-bit code from four character values.
//!
//! The resulting value is built with a byte order such that the string
//! being readable in expected order when viewed in a hex editor, if the value
//! is treated as a 32-bit little endian value.
#define FOUR_CHAR_CODE(a, b, c, d) (((d) << 24) | ((c) << 16) | ((b) << 8) | ((a)))

#if (defined(DEBUG))
static inline void debug_printf(const char *format, ...);

//! @brief Debug print utility.
//!
//! This print function will only output text when the @a DEBUG macro is defined.
static inline void debug_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
#else // (DEBUG || _DEBUG) && !DEBUG_PRINT_DISABLE
// Empty macro to cause debug_printf() calls to disappear.
#define debug_printf(x, ...) \
    do                       \
    {                        \
    } while (false)
#endif // (DEBUG || _DEBUG) && !DEBUG_PRINT_DISABLE

//! @brief Callback function invoked for a pin change interrupt.
//!
//! @ingroup bl_hw
typedef void (*pin_irq_callback_t)(uint32_t instance);

//! @brief Bootloader status group numbers.
//!
//! @ingroup bl_core
enum _bl_status_groups
{
    kStatusGroup_Bootloader = kStatusGroup_ApplicationRangeStart, //!< Bootloader status group number (100).
    kStatusGroup_SBLoader,                                        //!< SB loader status group number (101).
    kStatusGroup_MemoryInterface,                                 //!< Memory interface status group number (102).
    kStatusGroup_PropertyStore,                                   //!< Property store status group number (103).
    kStatusGroup_AppCrcCheck,                                     //!< Application crc check status group number (104).
    kStatusGroup_Packetizer,                                      //!< Packetizer status group number (105).
    kStatusGroup_ReliableUpdate                                   //!< Reliable Update status groupt number (106).
};

//! @brief Driver status group numbers.
//!
//! @ingroup bl_core
enum _bl_driver_status_groups
{
    kStatusGroup_QuadSPIDriver = 4, //!< QSPI driver status group number.
    kStatusGroup_OTFADDriver = 5,   //!< OTFAD driver status group number.
};

//! @brief Structure of version property.
//!
//! @ingroup bl_core
typedef union StandardVersion
{
    struct
    {
        uint8_t bugfix; //!< bugfix version [7:0]
        uint8_t minor;  //!< minor version [15:8]
        uint8_t major;  //!< major version [23:16]
        char name;      //!< name [31:24]
    };
    uint32_t version; //!< combined version numbers
} standard_version_t;

// #define MAKE_VERSION(bugfix, minor, major, name) (((name) << 24) | ((major) << 16) | ((minor) << 8 ) | (bugfix))

//! @brief External memory identifiers.
//!
//! @ingroup bl_core
enum _external_mem_identifiers
{
    kExternalMemId_QuadSPI0 = 1,
};

//! @brief Bootloader clock option
typedef enum _bootloader_clock_option
{
    kClockOption_EnterBootloader = 0, //!< Clock option for entering bootloader
    kClockOption_ExitBootloader = 1,  //!< Clock option for exiting bootloader
} bootloader_clock_option_t;

////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////

//! @addtogroup bl_hw
//! @{

//! @brief Initialize the hardware such as pinmux.
void init_hardware(void);

//! @brief DeInitialize the hardware such as disabling port clock gate
void deinit_hardware(void);

//! @brief Update available peripherals based on specific chips
void update_available_peripherals(void);

//! @brief Returns the logic level of the board specific GPIO pin used for autobaud.
// uint32_t read_autobaud_pin(uint32_t instance);

//! @brief Configure hardware clocks.
void configure_clocks(bootloader_clock_option_t option);

//! @brief Returns the available lirc clock frequency in Hertz.
uint32_t get_available_lirc_clock(void);

//! @brief Returns the current bus clock frequency in Hertz.
uint32_t get_bus_clock(void);

//! @brief Returns the current core clock frequency in Hertz.
uint32_t get_system_core_clock(void);

//! @brief Configure usb clock
bool usb_clock_init(void);

//! @brief Returns the value in MHz of the UART clock based on the instance.
uint32_t get_uart_clock(uint32_t instance);

//! @brief Returns true if reset BOOTROM mode is selected.
bool is_boot_pin_asserted(void);

//! @brief Enables the autobaud pin IRQ for the specific instance passed.
void enable_autobaud_pin_irq(uint32_t instance, pin_irq_callback_t func);

//! @brief Disables the autobaud pin IRQ for the instance passed.
void disable_autobaud_pin_irq(uint32_t instance);

//! @brief Declaration for the reset handler, which is defined in assembler.
void Reset_Handler(void);

//! @brief Initialize watchdog
void bootloader_watchdog_init(void);

//! @brief Service watchdog
void bootloader_watchdog_service(void);

//! @brief De-initialize watchdog
void bootloader_watchdog_deinit(void);

//! @brief Determine if QSPI module to be configured.
bool qspi_need_configure(void);

//! @brief Initialize QSPI and OTFAD module.
//! @param none
status_t otfad_init_as_needed(void);

//! @brief Bypass OTFAD module as needed.
//! @param none
status_t otfad_bypass_as_needed(void);

//! @brief Resume OTFAD module as needed.
//! @param none
status_t oftfad_resume_as_needed(void);

//! @brief Determine if QSPI memory is present or not.
bool is_qspi_present(void);

//! @brief Determine if OTFAD module is present or not.
bool is_otfad_present(void);

//! @brief Determine if LTC module is present or not.
bool is_ltc_present(void);

//! @brief Return status for intializing qspi and otfad modules
status_t get_qspi_otfad_init_status(void);

//!@bief Update status for intializing qspi and otfad modules
void update_qspi_otfad_init_status(status_t initStatus);

//!@brief Determine is the Secondary I2C slave address is enabled.
bool is_secondary_i2c_slave_address_enabled(void);

//!@brief Check if data to be accessed is in execute-only region.
bool is_in_execute_only_region(uint32_t start, uint32_t lengthInBytes);

//!@brief Check if second core is present.
bool is_second_core_present(void);

//! @}

#endif // __BOOTLOADER_COMMON_H__
