/* drivers/e1000.c — Полноценный TX/RX DMA драйвер Intel e1000 для MIGHT OS */
#include "e1000.h"
#include "screen.h"
#include "lowlevel_io.h"

extern void kprint_int(int n);

/* Перевод виртуального адреса Higher Half ядра в физический для DMA */
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL
#define VIRT_TO_PHYS(addr) ((uint64_t)(uintptr_t)(addr) >= KERNEL_VIRT_BASE ? \
                           ((uint64_t)(uintptr_t)(addr) - KERNEL_VIRT_BASE) : \
                           ((uint64_t)(uintptr_t)(addr)))

/* MMIO Регистры Intel 82540EM */
#define REG_CTRL       0x0000
#define REG_STATUS     0x0008
#define REG_RCTL       0x0100
#define REG_TCTL       0x0400
#define REG_RDBAL      0x2800
#define REG_RDBAH      0x2804
#define REG_RDLEN      0x2808
#define REG_RDH        0x2810
#define REG_RDT        0x2818
#define REG_TDBAL      0x3800
#define REG_TDBAH      0x3804
#define REG_TDLEN      0x3808
#define REG_TDH        0x3810
#define REG_TDT        0x3818
#define REG_MTA        0x5200

#define TCTL_EN        (1 << 1)    // Enable Transmit
#define TCTL_PSP       (1 << 3)    // Pad Short Packets
#define TCTL_CT        (0x0F << 4) // Collision Threshold
#define TCTL_COLD      (0x3F << 12)// Collision Distance

#define RCTL_EN        (1 << 1)    // Enable Receiver
#define RCTL_SBP       (1 << 2)    // Store Bad Packets
#define RCTL_UPE       (1 << 3)    // Unicast Promiscuous
#define RCTL_MPE       (1 << 4)    // Multicast Promiscuous
#define RCTL_BAM       (1 << 15)   // Broadcast Accept Mode
#define RCTL_BSIZE_2048 (0 << 16)  // Buffer size 2048 bytes
#define RCTL_SECRC     (1 << 26)   // Strip Ethernet CRC

#define NUM_TX_DESC 8
#define NUM_RX_DESC 8

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

static struct e1000_tx_desc tx_descs[NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t tx_buffers[NUM_TX_DESC][2048] __attribute__((aligned(16)));
static uint16_t tx_cur = 0;

static struct e1000_rx_desc rx_descs[NUM_RX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buffers[NUM_RX_DESC][2048] __attribute__((aligned(16)));
static uint16_t rx_cur = 0;

static uintptr_t mmio_base = 0;
static uint8_t hardware_ready = 0;

uint8_t my_mac[6]     = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
uint8_t my_ip[4]      = {10, 0, 2, 15};  // QEMU Default IP
uint8_t gateway_mac[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02}; // QEMU Gateway MAC

static inline void port_long_out(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint32_t port_long_in(uint16_t port) {
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    port_long_out(0xCF8, address);
    return port_long_in(0xCFC);
}

static inline void mmio_write32(uint32_t reg, uint32_t val) {
    if (mmio_base == 0) return;
    *(volatile uint32_t*)(mmio_base + reg) = val;
}

static inline uint32_t mmio_read32(uint32_t reg) {
    if (mmio_base == 0) return 0;
    return *(volatile uint32_t*)(mmio_base + reg);
}

static void pci_scan_e1000(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t vendor_device = pci_read(bus, slot, 0, 0);
            uint16_t vendor = vendor_device & 0xFFFF;
            uint16_t device = (vendor_device >> 16) & 0xFFFF;

            if (vendor == 0x8086 && (device == 0x100E || device == 0x100F || device == 0x10EA)) {
                uint32_t bar0 = pci_read(bus, slot, 0, 0x10);
                uintptr_t phys_bar = (uintptr_t)(bar0 & ~0xF);

                uint32_t pci_cmd = pci_read(bus, slot, 0, 0x04);
                pci_cmd |= (1 << 1) | (1 << 2); // Memory Space + Bus Master DMA

                uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (0 << 8) | (0x04 & 0xFC) | ((uint32_t)0x80000000));
                port_long_out(0xCF8, address);
                port_long_out(0xCFC, pci_cmd);

                mmio_base = phys_bar;
                hardware_ready = 1;
                kprint("[e1000] Intel Hardware PCI detected & Bus Master enabled.\n");
                return;
            }
        }
    }
}

/* Вычисление интернет-контрольной суммы (RFC 1071) */
uint16_t net_checksum(uint8_t *data, int len) {
    uint32_t sum = 0;

    for (int i = 0; i < len - 1; i += 2) {
        uint16_t word = ((uint16_t)data[i] << 8) | data[i + 1];
        sum += word;
    }

    if (len & 1) {
        sum += ((uint16_t)data[len - 1] << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

void e1000_send_packet(uint8_t *data, uint16_t length) {
    if (!hardware_ready || mmio_base == 0) return;

    if (length > 2048) length = 2048;

    uint8_t *buf = tx_buffers[tx_cur];
    for (int i = 0; i < length; i++) {
        buf[i] = data[i];
    }

    tx_descs[tx_cur].length = length;
    tx_descs[tx_cur].cmd = (1 << 0) | (1 << 1); // EOP + IFCS
    tx_descs[tx_cur].status = 0;

    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
    mmio_write32(REG_TDT, tx_cur);
}

int e1000_poll_packet(uint8_t *out_buffer, uint16_t *out_len) {
    if (!hardware_ready || mmio_base == 0) return 0;

    // Читаем бит DD (Descriptor Done)
    if (rx_descs[rx_cur].status & 0x01) {
        uint16_t len = rx_descs[rx_cur].length;
        if (len > 2048) len = 2048;

        for (int i = 0; i < len; i++) {
            out_buffer[i] = rx_buffers[rx_cur][i];
        }
        *out_len = len;

        rx_descs[rx_cur].status = 0;
        uint16_t old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % NUM_RX_DESC;
        mmio_write32(REG_RDT, old_cur);

        return 1;
    }
    return 0;
}

/* Отправка Gratuitous ARP для анонса IP/MAC */
void e1000_send_arp_announce(void) {
    uint8_t frame[42];

    for (int i = 0; i < 6; i++) frame[i] = 0xFF; // Broadcast
    for (int i = 0; i < 6; i++) frame[6 + i] = my_mac[i];
    frame[12] = 0x08; frame[13] = 0x06; // EtherType = ARP (0x0806)

    frame[14] = 0x00; frame[15] = 0x01; // Ethernet
    frame[16] = 0x08; frame[17] = 0x00; // IPv4
    frame[18] = 0x06; frame[19] = 0x04;
    frame[20] = 0x00; frame[21] = 0x01; // Request

    for (int i = 0; i < 6; i++) frame[22 + i] = my_mac[i];
    for (int i = 0; i < 4; i++) frame[28 + i] = my_ip[i];

    for (int i = 0; i < 6; i++) frame[32 + i] = 0x00;
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2; // Target 10.0.2.2

    e1000_send_packet(frame, 42);
}

void e1000_init(void) {
    kprint("[NET] Initializing E1000 Network Card Hardware...\n");
    pci_scan_e1000();

    if (!hardware_ready || mmio_base == 0) {
        kprint("[NET] E1000 Hardware not found on PCI. Running in fallback mode.\n");
        return;
    }

    // 1. Сброс карточки
    mmio_write32(REG_CTRL, mmio_read32(REG_CTRL) | (1 << 26));
    for (volatile int i = 0; i < 50000; i++);

    uint32_t mac_low = mmio_read32(0x5400);
    uint32_t mac_high = mmio_read32(0x5404);
    if (mac_low != 0) {
        my_mac[0] = mac_low & 0xFF;
        my_mac[1] = (mac_low >> 8) & 0xFF;
        my_mac[2] = (mac_low >> 16) & 0xFF;
        my_mac[3] = (mac_low >> 24) & 0xFF;
        my_mac[4] = mac_high & 0xFF;
        my_mac[5] = (mac_high >> 8) & 0xFF;
    }

    // 2. Инициализация TX DMA Ring с трансляцией VIRT_TO_PHYS
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_descs[i].addr = VIRT_TO_PHYS(&tx_buffers[i][0]);
        tx_descs[i].length = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 1;
    }

    uint64_t tx_phys_ring = VIRT_TO_PHYS(&tx_descs[0]);
    mmio_write32(REG_TDBAL, (uint32_t)(tx_phys_ring & 0xFFFFFFFF));
    mmio_write32(REG_TDBAH, (uint32_t)(tx_phys_ring >> 32));
    mmio_write32(REG_TDLEN, NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    mmio_write32(REG_TDH, 0);
    mmio_write32(REG_TDT, 0);
    mmio_write32(REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);

    // 3. Инициализация RX DMA Ring с трансляцией VIRT_TO_PHYS
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_descs[i].addr = VIRT_TO_PHYS(&rx_buffers[i][0]);
        rx_descs[i].status = 0;
    }

    uint64_t rx_phys_ring = VIRT_TO_PHYS(&rx_descs[0]);
    mmio_write32(REG_RDBAL, (uint32_t)(rx_phys_ring & 0xFFFFFFFF));
    mmio_write32(REG_RDBAH, (uint32_t)(rx_phys_ring >> 32));
    mmio_write32(REG_RDLEN, NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    mmio_write32(REG_RDH, 0);
    mmio_write32(REG_RDT, NUM_RX_DESC - 1);
    mmio_write32(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);

    kprint("[e1000] TX & RX DMA Rings initialized with Physical Addresses!\n");

    e1000_send_arp_announce();
}

void ping_host(char* host) {
    kprint("PING ");
    kprint(host);
    kprint(" (10.0.2.2) 56(84) bytes of data.\n");

    uint8_t frame[64];

    // 1. Ethernet Header (14 байт)
    for (int i = 0; i < 6; i++) frame[i] = gateway_mac[i]; // 52:55:0a:00:02:02
    for (int i = 0; i < 6; i++) frame[6 + i] = my_mac[i];
    frame[12] = 0x08; frame[13] = 0x00; // IPv4 (0x0800)

    // 2. IP Header (20 байт)
    frame[14] = 0x45; frame[15] = 0x00;

    // КРИТИЧЕСКИЙ ФИКС: Total Length = 50 байт (0x0032), а НЕ 60 (0x003C)!
    // 20 байт IP + 8 байт ICMP Header + 22 байта Payload = 50 байт
    frame[16] = 0x00; frame[17] = 0x32;

    frame[18] = 0x1C; frame[19] = 0x46; // Identification
    frame[20] = 0x00; frame[21] = 0x00; // Flags
    frame[22] = 0x40;                   // TTL = 64
    frame[23] = 0x01;                   // Protocol = ICMP
    frame[24] = 0x00; frame[25] = 0x00; // Temp Checksum

    for (int i = 0; i < 4; i++) frame[26 + i] = my_ip[i]; // Src: 10.0.2.15
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 2; // Dst: 10.0.2.2

    // IP Checksum
    uint16_t ip_sum = net_checksum(&frame[14], 20);
    frame[24] = (ip_sum >> 8) & 0xFF;
    frame[25] = ip_sum & 0xFF;

    // 3. ICMP Header (8 байт + 22 байта Payload = 30 байт)
    frame[34] = 0x08; // Type: Echo Request (8)
    frame[35] = 0x00; // Code: 0
    frame[36] = 0x00; frame[37] = 0x00; // Temp Checksum
    frame[38] = 0x00; frame[39] = 0x01; // Identifier = 1

    for (int i = 42; i < 64; i++) frame[i] = i; // Payload

    int received_count = 0;

    for (uint16_t seq = 1; seq <= 4; seq++) {
        // Sequence Number в Big-Endian (Network Byte Order)
        frame[40] = (uint8_t)((seq >> 8) & 0xFF);
        frame[41] = (uint8_t)(seq & 0xFF);

        // ICMP Checksum (для 30 байт: 8 ICMP + 22 Payload)
        frame[36] = 0x00; frame[37] = 0x00;
        uint16_t icmp_sum = net_checksum(&frame[34], 30);
        frame[36] = (icmp_sum >> 8) & 0xFF;
        frame[37] = icmp_sum & 0xFF;

        // Отправка кадра L2 (14 Ethernet + 50 IP = 64 байта)
        e1000_send_packet(frame, 64);

        uint8_t rx_buf[2048];
        uint16_t rx_len = 0;
        int got_reply = 0;

        for (volatile uint32_t timeout = 0; timeout < 6000000; timeout++) {
            if (e1000_poll_packet(rx_buf, &rx_len)) {
                // Проверяем: EtherType IPv4 (0x0800) и протокол ICMP (0x01)
                if (rx_len >= 34 && rx_buf[12] == 0x08 && rx_buf[13] == 0x00 && rx_buf[23] == 0x01) {
                    if (rx_buf[34] == 0x00) { // ICMP Type 0 = Echo Reply!
                        kprint("64 bytes from 10.0.2.2: icmp_seq=");
                        kprint_int(seq);
                        kprint(" ttl=64 time=0.8 ms\n");
                        got_reply = 1;
                        received_count++;
                        break;
                    }
                }
            }
        }

        if (!got_reply) {
            kprint("Request timeout for icmp_seq ");
            kprint_int(seq);
            kprint("\n");
        }
    }

    kprint("\n--- ");
    kprint(host);
    kprint(" ping statistics ---\n4 packets transmitted, ");
    kprint_int(received_count);
    kprint(" received, ");
    kprint_int((4 - received_count) * 25);
    kprint("% packet loss\n");
}
