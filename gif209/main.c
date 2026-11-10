/* ============================================================
 * GIF209 — Open Source STM32 LED Controller
 * Author : Saeid Moghadam
 * Version: 1.0 (2026)
 * Web    : https://soifgo.github.io/gif209/gif209.html
 *
 * GIF209 on STM32F030K6T6 (LQFP32) — 19 channels
 *   PA13/PA14 (SWD) are used as LED outputs (ch3/ch4), so SWD is NOT
 *   available. Flash via the BOOT0 jumper + UART bootloader (PA9/PA10).
 *   Bluetooth (USART1) -> 19 x WS2812 channels, record/play from SD
 *
 *  AUTO logic (INVERTED):
 *    auto_play = 0  ->  AUTO ON  (play LOOP on boot)
 *    auto_play = 1  ->  AUTO OFF (wait for command)
 *
 *  Line format (new): AARRGGBBSSEE...AARRGGBBSSEE[wait=N]
 *    up to 41 commands (492 chars) + optional wait=N at the END of the line.
 *    Commands run first, then wait (flush + clear untouched channels + delay).
 * ============================================================ */

#define UART_BAUD    115200u
#define BASE_SECTOR  2000u
#define MAX_LINE     511
#define MAX_FRAMES   60000u
#define STRIP_LEN    100

#define NUM_CH       19

#include <stdint.h>

/* ---------------- registers ---------------- */
#define RCC_CR        (*(volatile uint32_t *)0x40021000)
#define RCC_CFGR      (*(volatile uint32_t *)0x40021004)
#define RCC_AHBENR    (*(volatile uint32_t *)0x40021014)
#define RCC_APB2ENR   (*(volatile uint32_t *)0x40021018)
#define FLASH_ACR     (*(volatile uint32_t *)0x40022000)

#define GPIOA_MODER   (*(volatile uint32_t *)0x48000000)
#define GPIOA_OSPEEDR (*(volatile uint32_t *)0x48000008)
#define GPIOA_PUPDR   (*(volatile uint32_t *)0x4800000C)
#define GPIOA_BSRR    (*(volatile uint32_t *)0x48000018)
#define GPIOA_AFRL    (*(volatile uint32_t *)0x48000020)
#define GPIOA_AFRH    (*(volatile uint32_t *)0x48000024)

#define SPI1_CR1      (*(volatile uint32_t *)0x40013000)
#define SPI1_CR2      (*(volatile uint32_t *)0x40013004)
#define SPI1_SR       (*(volatile uint32_t *)0x40013008)
#define SPI1_DR8      (*(volatile uint8_t  *)0x4001300C)

#define USART1_CR1    (*(volatile uint32_t *)0x40013800)
#define USART1_BRR    (*(volatile uint32_t *)0x4001380C)
#define USART1_ISR    (*(volatile uint32_t *)0x4001381C)
#define USART1_ICR    (*(volatile uint32_t *)0x40013820)
#define USART1_RDR    (*(volatile uint32_t *)0x40013824)
#define USART1_TDR    (*(volatile uint32_t *)0x40013828)
#define USART1_CR3    (*(volatile uint32_t *)0x40013808)

/* DMA1 channel 3 = USART1_RX (default mapping on F030) */
#define DMA1_CCR3     (*(volatile uint32_t *)0x40020030)
#define DMA1_CNDTR3   (*(volatile uint32_t *)0x40020034)
#define DMA1_CPAR3    (*(volatile uint32_t *)0x40020038)
#define DMA1_CMAR3    (*(volatile uint32_t *)0x4002003C)

#define RX_RING 256u                 /* must be a power of 2 */
static uint8_t  rx_ring[RX_RING];
static uint16_t rx_tail;
static uint8_t  stop_idx;

#define SYST_CSR      (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR      (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR      (*(volatile uint32_t *)0xE000E018)

/* ---------------- clock: HSI/2 * 12 = 48 MHz ---------------- */
static void clock_init_48mhz(void)
{
    FLASH_ACR = 0x11;
    RCC_CFGR &= ~(0xFu << 18);
    RCC_CFGR |=  (10u << 18);
    RCC_CR   |=  (1u << 24);
    while (!(RCC_CR & (1u << 25)));
    RCC_CFGR &= ~3u;
    RCC_CFGR |=  2u;
    while ((RCC_CFGR & 0xC) != 0x8);
}

static void systick_init(void)
{
    SYST_RVR = 48000 - 1;
    SYST_CVR = 0;
    SYST_CSR = 5;
}

static void delay_ms(uint32_t n)
{
    while (n--) {
        (void)SYST_CSR;
        while (!(SYST_CSR & (1u << 16)));
    }
}

/* ---------------- UART ---------------- */
static void Bluetooth_Init(void)
{
    RCC_APB2ENR |= (1u << 14);

    GPIOA_MODER &= ~((3u << 18) | (3u << 20));
    GPIOA_MODER |=  ((2u << 18) | (2u << 20));
    GPIOA_AFRH  &= ~((0xFu << 4) | (0xFu << 8));
    GPIOA_AFRH  |=  ((1u << 4) | (1u << 8));

    USART1_BRR = (48000000u + UART_BAUD / 2u) / UART_BAUD;
    USART1_CR3 = (1u << 6) | (1u << 12);          /* DMAR + OVRDIS */

    /* circular DMA: hardware keeps filling rx_ring even while we are busy */
    RCC_AHBENR |= (1u << 0);                       /* DMA1 clock */
    DMA1_CCR3   = 0;
    DMA1_CPAR3  = 0x40013824u;                     /* USART1_RDR */
    DMA1_CMAR3  = (uint32_t)(uintptr_t)rx_ring;
    DMA1_CNDTR3 = RX_RING;
    DMA1_CCR3   = (1u << 7) | (1u << 5) | (1u << 0); /* MINC | CIRC | EN */
    rx_tail = 0;

    USART1_CR1 = (1u << 0) | (1u << 2) | (1u << 3);
}

static inline uint16_t rx_head(void)
{
    return (uint16_t)(RX_RING - DMA1_CNDTR3) & (RX_RING - 1u);
}

static void Bluetooth_SendChar(char c)
{
    while (!(USART1_ISR & (1u << 7)));
    USART1_TDR = (uint8_t)c;
}

static void Bluetooth_SendString(const char *s)
{
    while (*s) Bluetooth_SendChar(*s++);
}

static char Bluetooth_ReceiveChar(void)
{
    for (;;) {
        if (rx_tail != rx_head()) {
            uint8_t c = rx_ring[rx_tail];
            rx_tail = (uint16_t)((rx_tail + 1u) & (RX_RING - 1u));
            return (char)c;
        }
        if (USART1_ISR & 0x0E) USART1_ICR = 0x0E;
    }
}

static void Bluetooth_SendNum(uint32_t v)
{
    char t[11]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) Bluetooth_SendChar(t[--n]);
}

static void Bluetooth_SendHex(uint32_t v)
{
    const char *h = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4)
        Bluetooth_SendChar(h[(v >> i) & 15]);
}

/* ---------------- SPI + SD card ---------------- */
#define CS_HIGH()  (GPIOA_BSRR = (1u << 4))
#define CS_LOW()   (GPIOA_BSRR = (1u << (4 + 16)))

static uint8_t sd_hc;
static uint8_t sd_r1;
#define SD_RUN_BR  1  /* 48MHz / 4 = 12 MHz ; 1 => 24 MHz (if SD errors, set back to 2) */

static void spi_config(uint32_t br)
{
    SPI1_CR1 = 0;
    SPI1_CR2 = (7u << 8) | (1u << 12);
    SPI1_CR1 = (br << 3) | (1u << 2) | (1u << 8) | (1u << 9);
    SPI1_CR1 |= (1u << 6);
}

static void spi_init(void)
{
    RCC_APB2ENR |= (1u << 12);

    GPIOA_MODER   &= ~(0xFFu << 8);
    GPIOA_MODER   |=  (1u << 8) | (2u << 10) | (2u << 12) | (2u << 14);
    GPIOA_OSPEEDR |=  (0xFFu << 8);
    GPIOA_PUPDR   &= ~(3u << 12);
    GPIOA_PUPDR   |=  (1u << 12);
    GPIOA_AFRL    &= ~(0xFFFu << 20);

    CS_HIGH();
    spi_config(6);
}

static uint8_t spi_xfer(uint8_t b)
{
    while (!(SPI1_SR & (1u << 1)));
    SPI1_DR8 = b;
    while (!(SPI1_SR & (1u << 0)));
    return SPI1_DR8;
}

/* Pipelined block read: keeps the SPI FIFO busy instead of waiting per byte */
static void spi_read_block(uint8_t *buf, uint16_t n)
{
    uint16_t sent = 0, got = 0;
    while (got < n) {
        if (sent < n && (uint16_t)(sent - got) < 3 && (SPI1_SR & (1u << 1))) {
            SPI1_DR8 = 0xFF;
            sent++;
        }
        if (SPI1_SR & (1u << 0)) buf[got++] = SPI1_DR8;
    }
}

static void sd_release(void)
{
    CS_HIGH();
    spi_xfer(0xFF);
}

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    spi_xfer(0xFF);
    spi_xfer(0x40 | cmd);
    spi_xfer((uint8_t)(arg >> 24));
    spi_xfer((uint8_t)(arg >> 16));
    spi_xfer((uint8_t)(arg >> 8));
    spi_xfer((uint8_t)arg);
    spi_xfer(crc);
    for (int i = 0; i < 200; i++) {
        uint8_t r = spi_xfer(0xFF);
        if (!(r & 0x80)) return r;
    }
    return 0xFF;
}

static uint8_t sd_init(void)
{
    uint8_t r, ocr[4], v2 = 0;
    uint32_t t;

    spi_init();
    for (int i = 0; i < 10; i++) spi_xfer(0xFF);

    CS_LOW();
    r = sd_cmd(0, 0, 0x95);
    if (r != 0x01) { sd_r1 = r; sd_release(); return 1; }

    r = sd_cmd(8, 0x1AA, 0x87);
    if (r == 0x01) {
        for (int i = 0; i < 4; i++) ocr[i] = spi_xfer(0xFF);
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) { sd_r1 = ocr[3]; sd_release(); return 2; }
        v2 = 1;
    }

    t = 2000;
    uint8_t noresp = 0;
    do {
        sd_cmd(55, 0, 0xFF);
        r = sd_cmd(41, v2 ? 0x40000000u : 0, 0xFF);
        if (r == 0) break;
        if (r == 0xFF && ++noresp > 50) break;
        delay_ms(1);
    } while (--t);
    if (r != 0) { sd_r1 = r; sd_release(); return 3; }

    sd_hc = 0;
    if (v2) {
        r = sd_cmd(58, 0, 0xFF);
        if (r != 0) { sd_r1 = r; sd_release(); return 4; }
        for (int i = 0; i < 4; i++) ocr[i] = spi_xfer(0xFF);
        sd_hc = (ocr[0] & 0x40) ? 1 : 0;
    }
    if (!sd_hc) {
        r = sd_cmd(16, 512, 0xFF);
        if (r != 0) { sd_r1 = r; sd_release(); return 5; }
    }
    sd_release();

    spi_config(SD_RUN_BR);
    return 0;
}

static uint8_t sd_read(uint32_t sector, uint8_t *buf)
{
    uint8_t r = 0xFF;
    CS_LOW();
    sd_r1 = sd_cmd(17, sd_hc ? sector : (sector << 9), 0xFF);
    if (sd_r1 != 0) { sd_release(); return 1; }
    for (uint32_t t = 100000; t; t--) {
        r = spi_xfer(0xFF);
        if (r != 0xFF) break;
    }
    if (r != 0xFE) { sd_release(); return 2; }
    spi_read_block(buf, 512);
    spi_xfer(0xFF); spi_xfer(0xFF);
    sd_release();
    return 0;
}

static uint8_t sd_write(uint32_t sector, const uint8_t *buf)
{
    uint8_t r;
    CS_LOW();
    sd_r1 = sd_cmd(24, sd_hc ? sector : (sector << 9), 0xFF);
    if (sd_r1 != 0) { sd_release(); return 1; }
    spi_xfer(0xFF);
    spi_xfer(0xFE);
    for (int i = 0; i < 512; i++) spi_xfer(buf[i]);
    spi_xfer(0xFF); spi_xfer(0xFF);
    r = spi_xfer(0xFF);
    if ((r & 0x1F) != 0x05) { sd_release(); return 2; }
    uint32_t t;
    for (t = 200000; t; t--)
        if (spi_xfer(0xFF) == 0xFF) break;
    sd_release();
    return t ? 0 : 3;
}

/* Read multiple blocks using CMD18 */
static uint8_t sd_read_multi(uint32_t sector, uint8_t *buf, uint16_t count)
{
    uint8_t r = 0xFF;
    CS_LOW();
    sd_r1 = sd_cmd(18, sd_hc ? sector : (sector << 9), 0xFF);
    if (sd_r1 != 0) { sd_release(); return 1; }

    for (uint16_t i = 0; i < count; i++) {
        for (uint32_t t = 100000; t; t--) {
            r = spi_xfer(0xFF);
            if (r != 0xFF) break;
        }
        if (r != 0xFE) {
            CS_LOW(); sd_cmd(12, 0, 0xFF); sd_release();
            return 2;
        }
        spi_read_block(buf + i * 512, 512);
        spi_xfer(0xFF); spi_xfer(0xFF);
    }
    sd_cmd(12, 0, 0xFF);
    for (uint32_t t = 100000; t; t--)
        if (spi_xfer(0xFF) == 0xFF) break;
    sd_release();
    return 0;
}

/* ---------------- WS2812 driver ---------------- */
#define A_NOPS 16
#define B_NOPS 17
#define C_NOPS 15
#define STR_(x) #x
#define XSTR(x) STR_(x)

__attribute__((naked, noinline))
static void ws_send(const uint8_t *p, uint32_t n, uint32_t bsrr, uint32_t mask)
{
    __asm volatile(
        ".syntax unified            \n"
        "push   {r4, r5, r6}        \n"
        "lsls   r4, r3, #16         \n"
        "cmp    r1, #0              \n"
        "beq    9f                  \n"
    "10:                            \n"
        "ldrb   r5, [r0]            \n"
        "adds   r0, r0, #1          \n"
        "lsls   r5, r5, #24         \n"
        "movs   r6, #8              \n"
    "11:                            \n"
        "lsls   r5, r5, #1          \n"
        "str    r3, [r2]            \n"
        ".rept " XSTR(A_NOPS) "     \n"
        "nop                        \n"
        ".endr                      \n"
        "bcs    12f                 \n"
        "str    r4, [r2]            \n"
    "12:                            \n"
        ".rept " XSTR(B_NOPS) "     \n"
        "nop                        \n"
        ".endr                      \n"
        "str    r4, [r2]            \n"
        ".rept " XSTR(C_NOPS) "     \n"
        "nop                        \n"
        ".endr                      \n"
        "subs   r6, r6, #1          \n"
        "bne    11b                 \n"
        "subs   r1, r1, #1          \n"
        "bne    10b                 \n"
    "9:                             \n"
        "pop    {r4, r5, r6}        \n"
        "bx     lr                  \n"
    );
}

static void latch_gap(void)
{
    int32_t start = (int32_t)SYST_CVR;
    for (;;) {
        int32_t d = start - (int32_t)SYST_CVR;
        if (d < 0) d += 48000;
        if (d >= 14400) break;
    }
}

/* ---------------- channels ---------------- */
#define GPA 0x48000000u
#define GPB 0x48000400u
#define GPF 0x48001400u
typedef struct { uint32_t base; uint8_t pin; } chan_t;

/* Channel order around the LQFP32 package (PCB routing order):
 * ch1 = pin 21 ... ch10 = pin 30, then wraps to ch11 = pin 2 ... ch19 = pin 18. */
static const chan_t chan[NUM_CH] = {
    {GPA, 11},   /* ch1  PA11  pin 21 */
    {GPA, 12},   /* ch2  PA12  pin 22 */
    {GPA, 13},   /* ch3  PA13  pin 23 (former SWDIO) */
    {GPA, 14},   /* ch4  PA14  pin 24 (former SWCLK) */
    {GPA, 15},   /* ch5  PA15  pin 25 */
    {GPB, 3},    /* ch6  PB3   pin 26 */
    {GPB, 4},    /* ch7  PB4   pin 27 */
    {GPB, 5},    /* ch8  PB5   pin 28 */
    {GPB, 6},    /* ch9  PB6   pin 29 */
    {GPB, 7},    /* ch10 PB7   pin 30 */
    {GPF, 0},    /* ch11 PF0   pin 2 */
    {GPF, 1},    /* ch12 PF1   pin 3 */
    {GPA, 0},    /* ch13 PA0   pin 6 */
    {GPA, 1},    /* ch14 PA1   pin 7 */
    {GPA, 2},    /* ch15 PA2   pin 8 */
    {GPA, 3},    /* ch16 PA3   pin 9 */
    {GPB, 0},    /* ch17 PB0   pin 14 */
    {GPB, 1},    /* ch18 PB1   pin 15 */
    {GPA, 8}     /* ch19 PA8   pin 18 */
};

static uint8_t px[STRIP_LEN * 3];
static uint8_t touched[NUM_CH];
static uint8_t ao;
static uint8_t nch = NUM_CH;
static uint32_t play_delay = 0;
static uint8_t  auto_play = 0;
static uint8_t  boot_loop = 0;

static void channels_init(void)
{
    RCC_AHBENR |= (1u << 17) | (1u << 18) | (1u << 22);
    for (int k = 0; k < NUM_CH; k++) {
        volatile uint32_t *g = (volatile uint32_t *)chan[k].base;
        uint8_t pin = chan[k].pin;
        g[6] = 1u << (pin + 16);
        g[3] &= ~(3u << (pin * 2));                 /* no pull (PA13/PA14 reset to pull-up/down) */
        g[0] = (g[0] & ~(3u << (pin * 2))) | (1u << (pin * 2));
        g[2] |= 3u << (pin * 2);
    }
}

static void clear_px(void)
{
    for (uint16_t i = 0; i < STRIP_LEN * 3; i++) px[i] = 0;
}

static void send_channel(uint8_t k)
{
    const chan_t *c = &chan[k - 1];
    ws_send(px, STRIP_LEN * 3, c->base + 0x18, 1u << c->pin);
    latch_gap();
}

/* ---------------- ARGBSE interpreter ---------------- */
static uint8_t hexv(uint8_t c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}
static uint8_t hex2(const uint8_t *p) { return (uint8_t)((hexv(p[0]) << 4) | hexv(p[1])); }

static uint16_t slen(const char *s) { uint16_t n = 0; while (s[n]) n++; return n; }

/* position of first occurrence of w in s, or -1 */
static int16_t find_str(const char *s, const char *w)
{
    for (int16_t p = 0; s[p]; p++) {
        uint16_t i = 0;
        while (w[i] && s[p + i] == w[i]) i++;
        if (!w[i]) return p;
    }
    return -1;
}

static int has_str(const char *s, const char *w)
{
    return find_str(s, w) >= 0;
}

static uint32_t read_num_after(const char *s, const char *key)
{
    uint32_t v = 0;
    for (; *s; s++) {
        uint16_t i = 0;
        while (key[i] && s[i] == key[i]) i++;
        if (!key[i]) {
            s += i;
            while (*s >= '0' && *s <= '9') {
                v = v * 10 + (uint32_t)(*s - '0');
                s++;
                if (v > 1000000) v = 1000000;
            }
            return v;
        }
    }
    return 0;
}

static uint8_t  pend = 0;   /* channel whose pixels are painted but not yet sent */
static uint32_t lit  = 0;   /* bit (k-1) = channel k currently shows something   */

static void flush_px(void)
{
    if (pend) {
        send_channel(pend);
        lit |= 1u << (pend - 1);
        pend = 0;
    }
}

static void do_wait(void)
{
    flush_px();
    for (uint8_t k = 1; k <= NUM_CH; k++)       /* NUM_CH, not nch: an old SD header may still say 17 */
        if (!touched[k - 1] && (lit & (1u << (k - 1)))) {
            clear_px();
            send_channel(k);
            lit &= ~(1u << (k - 1));
        }
    for (uint8_t k = 0; k < NUM_CH; k++) touched[k] = 0;
    ao = 0;
}

/* Runs the commands that come BEFORE "wait" (if any), then do_wait().
 * Line: AARRGGBBSSEE...[wait=N]  or just  wait=N  */
static void execute_line(const char *line)
{
    int16_t  wp  = find_str(line, "wait");
    uint16_t len = (wp >= 0) ? (uint16_t)wp : slen(line);
    uint16_t n   = len / 12;
    uint16_t off = len - n * 12;
    if (off > 1) off = 1;

    for (uint16_t k = 0; k < n; k++) {
        const uint8_t *p = (const uint8_t *)line + off + k * 12;
        uint8_t a = hex2(p),     r = hex2(p + 2), g = hex2(p + 4);
        uint8_t b = hex2(p + 6), s = hex2(p + 8), e = hex2(p + 10);
        if (a < 1 || a > NUM_CH) continue;

        touched[a - 1] = 1;
        if (ao != a) { flush_px(); clear_px(); }
        ao = a;
        for (uint16_t i = s; i <= e && i < STRIP_LEN; i++) {
            px[i * 3]     = g;
            px[i * 3 + 1] = r;
            px[i * 3 + 2] = b;
        }
        pend = a;                 /* sent once per channel (flush_px) */
    }

    if (wp >= 0) do_wait();
}

/* ---------------- application ---------------- */
static uint8_t  blk[512];
static uint8_t  line[MAX_LINE + 1];
static uint8_t  sd_ok;
static uint8_t  recording;
static uint16_t frames;

#define MULTI_BATCH 2
static uint8_t multi_buf[MULTI_BATCH * 512];

static int is_cmd(const uint8_t *a, uint16_t len, const char *w)
{
    uint16_t i = 0;
    for (; w[i]; i++)
        if (i >= len || (a[i] | 0x20) != (w[i] | 0x20)) return 0;
    return i == len;
}

static uint8_t save_header(void)
{
    for (int i = 0; i < 512; i++) blk[i] = 0;
    blk[0] = 'R'; blk[1] = 'M'; blk[2] = 'G'; blk[3] = '2';
    blk[4] = (uint8_t)(frames & 0xFF);
    blk[5] = (uint8_t)(frames >> 8);
    blk[6] = nch;
    blk[7]  = (uint8_t)(play_delay & 0xFF);
    blk[8]  = (uint8_t)((play_delay >> 8) & 0xFF);
    blk[9]  = (uint8_t)((play_delay >> 16) & 0xFF);
    blk[10] = (uint8_t)((play_delay >> 24) & 0xFF);
    blk[11] = auto_play;
    return sd_write(BASE_SECTOR, blk);
}

static uint16_t load_header(void)
{
    if (sd_read(BASE_SECTOR, blk)) return 0;
    if (blk[0] != 'R' || blk[1] != 'M' || blk[2] != 'G' || blk[3] != '2') return 0;
    if (blk[6] >= 1 && blk[6] <= NUM_CH) nch = blk[6];
    play_delay = (uint32_t)blk[7] | ((uint32_t)blk[8] << 8) |
                 ((uint32_t)blk[9] << 16) | ((uint32_t)blk[10] << 24);
    auto_play = blk[11];
    if (auto_play > 1) auto_play = 0;
    return (uint16_t)(blk[4] | (blk[5] << 8));
}

static void report_error(const char *what, uint8_t e)
{
    Bluetooth_SendString(what);
    Bluetooth_SendNum(e);
    Bluetooth_SendString(" r1=");
    Bluetooth_SendHex(sd_r1);
    Bluetooth_SendString("\r\n");
}

static void uart_flush_rx(void)
{
    rx_tail = rx_head();
    if (USART1_ISR & 0x0E) USART1_ICR = 0x0E;
}

/* Drains the RX ring; returns 1 only when the word "STOP" (any case) arrives.
 * Everything else (e.g. "OK+LOST" sent by the BT module on disconnect, line
 * noise, CR/LF) is ignored. */
static int uart_stop_seen(void)
{
    static const char w[] = "stop";
    while (rx_tail != rx_head()) {
        uint8_t c = rx_ring[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1u) & (RX_RING - 1u));
        c |= 0x20;
        if (c == (uint8_t)w[stop_idx]) {
            if (++stop_idx == 4) { stop_idx = 0; return 1; }
        } else {
            stop_idx = (c == 's') ? 1 : 0;
        }
    }
    return 0;
}

/* delay that can be interrupted by STOP; returns 1 if stopped */
static int delay_ms_chk(uint32_t n)
{
    while (n--) {
        (void)SYST_CSR;
        while (!(SYST_CSR & (1u << 16)))
            if (uart_stop_seen()) return 1;
    }
    return 0;
}

/* -------- DUMP -------- */
static void dump_frame(uint16_t idx)
{
    Bluetooth_SendString("[");
    Bluetooth_SendNum(idx);
    Bluetooth_SendString("] ");
    for (int i = 0; i < 512; i++) {
        uint8_t c = blk[i];
        if (c == 0) break;
        if (c >= 32 && c < 127) Bluetooth_SendChar((char)c);
        else {
            Bluetooth_SendString("<");
            Bluetooth_SendHex(c);
            Bluetooth_SendString(">");
        }
    }
    Bluetooth_SendString("\r\n");
}

static void do_dump(uint16_t only)
{
    if (!sd_ok) { Bluetooth_SendString("SD not ready\r\n"); return; }
    uart_flush_rx(); delay_ms(50); uart_flush_rx();

    if (sd_read(BASE_SECTOR, blk)) { report_error("hdr read err=", sd_r1); return; }
    Bluetooth_SendString("HDR raw: ");
    for (int i = 0; i < 16; i++) { Bluetooth_SendHex(blk[i]); Bluetooth_SendChar(' '); }
    Bluetooth_SendString("\r\n");

    uint16_t n = (uint16_t)(blk[4] | (blk[5] << 8));
    uint32_t d = (uint32_t)blk[7] | ((uint32_t)blk[8] << 8) |
                 ((uint32_t)blk[9] << 16) | ((uint32_t)blk[10] << 24);
    Bluetooth_SendString("HDR frames=");
    Bluetooth_SendNum(n);
    Bluetooth_SendString(" nch=");
    Bluetooth_SendNum(blk[6]);
    Bluetooth_SendString(" delay=");
    Bluetooth_SendNum(d);
    Bluetooth_SendString(" auto=");
    Bluetooth_SendNum(blk[11]);
    Bluetooth_SendString("\r\n");

    if (n == 0 || n > MAX_FRAMES) { Bluetooth_SendString("(no frames)\r\n"); return; }

    if (only == 0xFFFF) {
        if (blk[0] != 'R' || blk[1] != 'M' || blk[2] != 'G' || blk[3] != '2') {
            Bluetooth_SendString("HDR tag BAD!\r\n");
            return;
        }
        for (uint16_t i = 0; i < n; i++) {
            uint8_t e = sd_read(BASE_SECTOR + 1 + i, blk);
            if (e) { report_error("read err=", e); return; }
            dump_frame(i);
        }
        Bluetooth_SendString("DUMP DONE\r\n");
    } else {
        if (only >= n) { Bluetooth_SendString("index out of range\r\n"); return; }
        uint8_t e = sd_read(BASE_SECTOR + 1 + only, blk);
        if (e) { report_error("read err=", e); return; }
        dump_frame(only);
    }
}

/* -------- PLAY / LOOP -------- */
static void play_all(uint8_t loop)
{
    uint16_t n = load_header();
    if (!n) { Bluetooth_SendString("EMPTY\r\n"); return; }

    uart_flush_rx();
    delay_ms(100);
    uart_flush_rx();
    stop_idx = 0;

    Bluetooth_SendString("PLAY n=");
    Bluetooth_SendNum(n);
    Bluetooth_SendString(" delay=");
    Bluetooth_SendNum(play_delay);
    Bluetooth_SendString("\r\n");

    do {
        uint16_t i = 0;
        while (i < n) {
            uint16_t batch = (n - i > MULTI_BATCH) ? MULTI_BATCH : (n - i);

            uint8_t e = sd_read_multi(BASE_SECTOR + 1 + i, multi_buf, batch);
            if (e) {
                e = sd_read(BASE_SECTOR + 1 + i, blk);
                if (e) { flush_px(); report_error("read error=", e); return; }
                batch = 1;
                for (int j = 0; j < 512; j++) multi_buf[j] = blk[j];
            }

            for (uint16_t b = 0; b < batch; b++) {
                if (uart_stop_seen()) {
                    flush_px();
                    Bluetooth_SendString("STOPPED\r\n");
                    return;
                }

                uint8_t *frame_data = multi_buf + b * 512;
                frame_data[511] = 0;

                int is_wait_line = has_str((const char *)frame_data, "wait");
                uint32_t w = is_wait_line ? read_num_after((const char *)frame_data, "wait=") : 0;

                execute_line((const char *)frame_data);

                uint32_t dly = (is_wait_line && w) ? w : play_delay;
                if (dly && delay_ms_chk(dly)) {
                    flush_px();
                    Bluetooth_SendString("STOPPED\r\n");
                    return;
                }
            }
            i += batch;
        }
    } while (loop);

    flush_px();
    Bluetooth_SendString("PLAY DONE\r\n");
}

static void handle_line(uint16_t len)
{
    line[len] = 0;

    /* Add STOP command */
    if (is_cmd(line, len, "STOP")) {
        Bluetooth_SendString("STOPPED\r\n");
        recording = 0;
        Bluetooth_SendString("next\r\n");   /* ack, so the phone can send the next line */
        return;
    }

    if (is_cmd(line, len, "REC") || is_cmd(line, len, "END") || is_cmd(line, len, "PLAY") ||
        is_cmd(line, len, "LOOP") || is_cmd(line, len, "INFO") || is_cmd(line, len, "ERASE") ||
        is_cmd(line, len, "DUMP") || is_cmd(line, len, "TEST5") || is_cmd(line, len, "TEST10")) {

        if (is_cmd(line, len, "TEST5")) {
            Bluetooth_SendString("START\r\n");
            delay_ms(5000);
            Bluetooth_SendString("END\r\n");
            return;
        }
        if (is_cmd(line, len, "TEST10")) {
            Bluetooth_SendString("START\r\n");
            delay_ms(10000);
            Bluetooth_SendString("END\r\n");
            return;
        }

        if (!sd_ok) { Bluetooth_SendString("SD not ready\r\n"); return; }

        if (is_cmd(line, len, "REC")) {
            frames = 0;
            uint8_t e = save_header();
            if (e) { recording = 0; report_error("write error=", e); return; }
            recording = 1;
            Bluetooth_SendString("REC OK\r\n");
        } else if (is_cmd(line, len, "END")) {
            recording = 0;
            uint8_t e = save_header();
            if (e) { report_error("write error=", e); return; }
            Bluetooth_SendString("SAVED ");
            Bluetooth_SendNum(frames);
            Bluetooth_SendString("\r\n");
        } else if (is_cmd(line, len, "PLAY")) {
            play_all(0);
        } else if (is_cmd(line, len, "LOOP")) {
            play_all(1);
        } else if (is_cmd(line, len, "DUMP")) {
            do_dump(0xFFFF);
        } else if (is_cmd(line, len, "ERASE")) {
            frames = 0;
            uint8_t e = save_header();
            if (e) { report_error("write error=", e); return; }
            Bluetooth_SendString("ERASED\r\n");
        } else {                                          /* INFO */
            uint16_t n = load_header();
            Bluetooth_SendString("frames=");
            Bluetooth_SendNum(n);
            Bluetooth_SendString(" ch=");
            Bluetooth_SendNum(nch);
            Bluetooth_SendString(" delay=");
            Bluetooth_SendNum(play_delay);
            Bluetooth_SendString(" auto=");
            Bluetooth_SendNum(auto_play);
            if (auto_play == 0) Bluetooth_SendString(" (ON)");
            else Bluetooth_SendString(" (OFF)");
            Bluetooth_SendString("\r\n");
        }
        Bluetooth_SendString("next\r\n");   /* ack for REC / END / PLAY / LOOP(after STOP) / INFO / ERASE / DUMP */
        return;
    }

    if (has_str((const char *)line, "DUMP=")) {
        uint32_t v = read_num_after((const char *)line, "DUMP=");
        do_dump((uint16_t)v);
        Bluetooth_SendString("next\r\n");
        return;
    }

    if (has_str((const char *)line, "CH=")) {
        uint32_t v = read_num_after((const char *)line, "CH=");
        if (v >= 1 && v <= NUM_CH) {
            nch = (uint8_t)v;
            if (sd_ok) save_header();
            Bluetooth_SendString("OK CH=");
            Bluetooth_SendNum(nch);
            Bluetooth_SendString("\r\n");
        }
        Bluetooth_SendString("next\r\n");
        return;
    }

    if (has_str((const char *)line, "DELAY=")) {
        play_delay = read_num_after((const char *)line, "DELAY=");
        if (sd_ok) save_header();
        Bluetooth_SendString("OK DELAY=");
        Bluetooth_SendNum(play_delay);
        Bluetooth_SendString("\r\n");
        Bluetooth_SendString("next\r\n");
        return;
    }

    if (has_str((const char *)line, "AUTO=")) {
        uint32_t v = read_num_after((const char *)line, "AUTO=");
        auto_play = (v > 0) ? 1 : 0;
        if (sd_ok) save_header();
        Bluetooth_SendString("OK AUTO=");
        Bluetooth_SendNum(auto_play);
        Bluetooth_SendString("\r\n");
        Bluetooth_SendString("next\r\n");
        return;
    }

    if (recording) {
        /* both command lines and (merged) wait lines are stored as-is, one per sector */
        if (frames >= MAX_FRAMES) { Bluetooth_SendString("FULL\r\n"); return; }
        for (uint16_t i = 0; i < 512; i++)
            blk[i] = (i < len) ? line[i] : 0;
        uint8_t e = sd_write(BASE_SECTOR + 1 + frames, blk);
        if (e) { report_error("write error=", e); return; }
        frames++;
        Bluetooth_SendString("next\r\n");
        return;
    }

    /* live mode: execute_line() flushes by itself when the line contains wait */
    execute_line((const char *)line);
    if (!has_str((const char *)line, "wait")) flush_px();
    Bluetooth_SendString("next\r\n");
}

static void Bluetooth_Task(void)
{
    static uint16_t len = 0;

    if (boot_loop) {
        boot_loop = 0;
        handle_line(4);
        return;
    }

    char c = Bluetooth_ReceiveChar();

    if (c == '\r' || c == '\n') {
        if (!len) return;
        uint16_t n = len;
        len = 0;
        handle_line(n);
    } else if (len < MAX_LINE) {
        line[len++] = (uint8_t)c;
    }
}

int main(void)
{
    clock_init_48mhz();
    systick_init();
    RCC_AHBENR |= (1u << 17);
    Bluetooth_Init();
    channels_init();
    for (uint8_t k = 1; k <= NUM_CH; k++) { clear_px(); send_channel(k); }

    delay_ms(300);
    uint8_t e = 1;
    for (uint8_t tries = 0; tries < 5 && e; tries++) {
        e = sd_init();
        if (e) delay_ms(200);
    }
    if (e) {
        Bluetooth_SendString("SD init FAIL step=");
        Bluetooth_SendNum(e);
        Bluetooth_SendString(" r1=");
        Bluetooth_SendHex(sd_r1);
        Bluetooth_SendString("\r\n");
    } else {
        sd_ok = 1;
        frames = load_header();
        Bluetooth_SendString("SD init OK\r\n");

        if (auto_play == 0 && frames > 0) {
            line[0] = 'L'; line[1] = 'O'; line[2] = 'O'; line[3] = 'P';
            boot_loop = 1;
            Bluetooth_SendString("AUTO LOOP QUEUED\r\n");
        } else {
            Bluetooth_SendString("AUTO OFF or EMPTY\r\n");
        }
    }

    for (;;)
        Bluetooth_Task();
}
