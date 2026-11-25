/* USB CDC accepts UPDATE; initial installation uses the ROM DFU bootloader. */
#include <stdint.h>

#include "tusb.h"

#define REG(address) (*(volatile uint32_t *)(address))

#define RCC_CR REG(0x40023800u)
#define RCC_PLLCFGR REG(0x40023804u)
#define RCC_CFGR REG(0x40023808u)
#define RCC_CIR REG(0x4002380Cu)
#define RCC_AHB1RSTR REG(0x40023810u)
#define RCC_AHB1ENR REG(0x40023830u)
#define RCC_AHB2ENR REG(0x40023834u)
#define RCC_AHB2RSTR REG(0x4002382Cu)
#define RCC_APB1RSTR REG(0x40023820u)
#define RCC_APB2RSTR REG(0x40023824u)
#define RCC_APB1ENR REG(0x40023840u)
#define RCC_APB2ENR REG(0x40023844u)
#define FLASH_ACR REG(0x40023C00u)

#define GPIOA_MODER REG(0x40020000u)
#define GPIOA_OSPEEDR REG(0x40020008u)
#define GPIOA_AFRH REG(0x40020024u)

#define SYSTICK_CTRL REG(0xE000E010u)
#define SYSTICK_LOAD REG(0xE000E014u)
#define SYSTICK_VAL REG(0xE000E018u)
#define SCB_ICSR REG(0xE000ED04u)
#define SCB_VTOR REG(0xE000ED08u)
#define NVIC_ICER0 REG(0xE000E180u)
#define NVIC_ICER1 REG(0xE000E184u)
#define NVIC_ICER2 REG(0xE000E188u)
#define NVIC_ICPR0 REG(0xE000E280u)
#define NVIC_ICPR1 REG(0xE000E284u)
#define NVIC_ICPR2 REG(0xE000E288u)

#define SYSTEM_MEMORY 0x1FFF0000u
#define USB_UPDATE_VID 0xCAFEu
#define USB_UPDATE_PID 0x4010u

uint32_t SystemCoreClock = 48000000u;
static volatile uint32_t system_millis;
static uint32_t command_matched;
static volatile uint8_t update_requested;

void SysTick_Handler(void)
{
    system_millis++;
}

uint32_t tusb_time_millis_api(void)
{
    return system_millis;
}

void OTG_FS_IRQHandler(void)
{
    tusb_int_handler(0, true);
}

static void clock_init(void)
{
    RCC_CR |= 1u;
    while ((RCC_CR & (1u << 1)) == 0u) {
    }

    RCC_CR |= 1u << 16;
    uint32_t timeout = 200000u;
    while ((RCC_CR & (1u << 17)) == 0u && --timeout != 0u) {
    }

    FLASH_ACR = (FLASH_ACR & ~7u) | 1u | (1u << 8) | (1u << 9) | (1u << 10);
    RCC_CFGR = (RCC_CFGR & ~(7u << 10)) | (4u << 10);
    const uint32_t pll_m = (RCC_CR & (1u << 17)) != 0u ? 25u : 16u;
    const uint32_t pll_source = (RCC_CR & (1u << 17)) != 0u ? 1u << 22 : 0u;
    RCC_PLLCFGR = pll_m | (192u << 6) | (1u << 16) | (4u << 24) | pll_source;
    RCC_CR |= 1u << 24;
    while ((RCC_CR & (1u << 25)) == 0u) {
    }

    RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
    while (((RCC_CFGR >> 2) & 3u) != 2u) {
    }

    RCC_AHB1ENR |= 1u;
    RCC_AHB2ENR |= 1u << 7;

    GPIOA_MODER = (GPIOA_MODER & ~((3u << 22) | (3u << 24)))
                | (2u << 22) | (2u << 24);
    GPIOA_OSPEEDR |= (3u << 22) | (3u << 24);
    GPIOA_AFRH = (GPIOA_AFRH & ~((0xFu << 12) | (0xFu << 16)))
               | (10u << 12) | (10u << 16);

    SYSTICK_LOAD = 48000u - 1u;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = 7u;
}

static void jump_to_system_bootloader(void)
    __attribute__((naked, noreturn));

static void jump_to_system_bootloader(void)
{
    __asm volatile (
        "ldr r0, =0x1FFF0000\n"
        "ldr r1, [r0]\n"
        "msr msp, r1\n"
        "ldr r0, =0x1FFF0004\n"
        "ldr r1, [r0]\n"
        "movs r2, #0\n"
        "msr control, r2\n"
        "msr basepri, r2\n"
        "msr faultmask, r2\n"
        "dsb\n"
        "isb\n"
        "cpsie i\n"
        "bx r1\n"
    );
}

static void enter_usb_dfu(void)
{
    (void)tud_disconnect();
    const uint32_t disconnect_start = system_millis;
    while ((uint32_t)(system_millis - disconnect_start) < 250u) {
    }

    __asm volatile ("cpsid i");

    SYSTICK_CTRL = 0;
    RCC_AHB2RSTR |= 1u << 7;
    RCC_AHB2RSTR &= ~(1u << 7);
    RCC_AHB1RSTR |= 1u;
    RCC_AHB1RSTR &= ~1u;
    RCC_APB1RSTR = 0xFFFFFFFFu;
    RCC_APB1RSTR = 0;
    RCC_APB2RSTR = 0xFFFFFFFFu;
    RCC_APB2RSTR = 0;

    RCC_CR |= 1u;
    while ((RCC_CR & (1u << 1)) == 0u) {
    }
    RCC_CFGR = 0;
    while ((RCC_CFGR & (3u << 2)) != 0u) {
    }
    RCC_CR &= ~((1u << 16) | (1u << 19) | (1u << 24));
    while ((RCC_CR & (1u << 25)) != 0u) {
    }
    RCC_PLLCFGR = 0x24003010u;
    RCC_CIR = 0;
    RCC_AHB1ENR = 0;
    RCC_AHB2ENR = 0;
    RCC_APB1ENR = 0;
    RCC_APB2ENR = 0;

    SCB_ICSR = (1u << 25) | (1u << 27);
    NVIC_ICER0 = 0xFFFFFFFFu;
    NVIC_ICER1 = 0xFFFFFFFFu;
    NVIC_ICER2 = 0xFFFFFFFFu;
    NVIC_ICPR0 = 0xFFFFFFFFu;
    NVIC_ICPR1 = 0xFFFFFFFFu;
    NVIC_ICPR2 = 0xFFFFFFFFu;
    SCB_VTOR = SYSTEM_MEMORY;
    __asm volatile ("dsb\nisb");
    jump_to_system_bootloader();
}

void tud_cdc_rx_cb(uint8_t instance)
{
    (void)instance;

    while (tud_cdc_available() != 0u) {
        const char received = (char)tud_cdc_read_char();
        static const char command[] = "UPDATE";

        if (received == command[command_matched]) {
            command_matched++;
            if (command_matched == sizeof(command) - 1u) {
                command_matched = 0;
                update_requested = 1u;
                tud_cdc_write_str("OK\r\n");
                tud_cdc_write_flush();
                return;
            }
        } else {
            command_matched = (received == command[0]) ? 1u : 0u;
        }
    }
}

int main(void)
{
    clock_init();

    const tusb_rhport_init_t usb_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };

    if (!tusb_init(0, &usb_init)) {
        for (;;) {
        }
    }

    for (;;) {
        tud_task();

        if (update_requested != 0u) {
            const uint32_t start = system_millis;
            while ((uint32_t)(system_millis - start) < 100u) {
                tud_task();
            }
            enter_usb_dfu();
        }
    }
}

static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_UPDATE_VID,
    .idProduct = USB_UPDATE_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&device_descriptor;
}

enum {
    CDC_CONTROL_INTERFACE = 0,
    CDC_DATA_INTERFACE,
    CDC_INTERFACE_COUNT
};

#define CDC_CONFIGURATION_LENGTH (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, CDC_INTERFACE_COUNT, 0,
                          CDC_CONFIGURATION_LENGTH, 0x80, 100),
    TUD_CDC_DESCRIPTOR(CDC_CONTROL_INTERFACE, 4, 0x81, 8,
                       0x02, 0x82, 64)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return configuration_descriptor;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t language_id)
{
    (void)language_id;
    static const char *const strings[] = {
        "\x09\x04",
        "PWM Demo",
        "USB Update CDC",
        "F401-USB-UPDATE",
        "CDC Interface"
    };
    static uint16_t descriptor[32];

    if (index >= sizeof(strings) / sizeof(strings[0])) {
        return 0;
    }

    uint8_t length = 0;
    if (index == 0u) {
        descriptor[1] = 0x0409u;
        length = 1u;
    } else {
        const char *text = strings[index];
        while (text[length] != '\0' && length < 31u) {
            descriptor[length + 1u] = (uint8_t)text[length];
            length++;
        }
    }

    descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2u * length + 2u));
    return descriptor;
}
