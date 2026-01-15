#include "arduboy_avr.h"

#include "arduboy2.h"
#include "arduboy_avr_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef ENABLE_OPENCV
#include <fstream>
#include <vector>
#include <string>
#include <cstdarg>
#endif

extern "C" {
#include "../driver/st7565.h"
#include "../misc.h"
#include "../ui/helper.h"
#include "../ui/ui.h"
}

extern "C" {
#undef ARRAY_SIZE
#include "avr_spi.h"
#include "sim_avr.h"
#include "sim_hex.h"
#include "sim_io.h"
#include "sim_time.h"
#include "avr_ioport.h"
#include "ssd1306_virt.h"
}

static avr_t *gArduboyAvr = NULL;
static ssd1306_t gArduboyDisplay;
static avr_irq_t *gButtonPortB4 = NULL;
static avr_irq_t *gButtonPortE6 = NULL;
static avr_irq_t *gButtonPortF4 = NULL;
static avr_irq_t *gButtonPortF5 = NULL;
static avr_irq_t *gButtonPortF6 = NULL;
static avr_irq_t *gButtonPortF7 = NULL;

static int gArduboyAvrDebugLevel = 0;

#ifdef ENABLE_OPENCV
static FILE *gArduboyAvrLogFile = nullptr;
static void ArduboyAvrLog(int level, const char *fmt, ...) {
    if (gArduboyAvrDebugLevel < level) {
        return;
    }
    FILE *out = gArduboyAvrLogFile ? gArduboyAvrLogFile : stderr;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    if (!gArduboyAvrLogFile || level <= 1) {
        fflush(out);
    }
}
#else
static void ArduboyAvrLog(int level, const char *fmt, ...) {
    (void)level;
    (void)fmt;
}
#endif

static bool gArduboyAvrInitialized = false;
static bool gArduboyAvrFrameReady = false;
static bool gArduboyAvrHasFrame = false;
static bool gArduboyAvrFirstFrameLogged = false;
static int gArduboyAvrLastCpuState = -1;
static uint8_t gArduboyAvrButtons = 0;
static const char kArduboyAvrFxWarning[] = "FX data missing";
static const char kArduboyAvrCpuHaltWarning[] = "ROM halted";
static const char *gArduboyAvrMenuWarning = NULL;

static bool gArduboyAvrFxReady = false;
static bool gArduboyAvrOledSelected = false;
static uint8_t gArduboyAvrFxCsLevel = 1;
static uint8_t gArduboyAvrOledCsLevel = 1;
static bool gArduboyAvrFxSelected = false;

static avr_irq_t *gArduboyAvrSpiOut = NULL;
static avr_irq_t *gArduboyAvrSpiIn = NULL;
static avr_irq_t *gArduboyAvrFxCs = NULL;
static char gArduboyAvrFxCsPort = 0;
static uint8_t gArduboyAvrFxCsBit = 0;
static avr_irq_t *gArduboyAvrOledPortD = NULL;

static constexpr char kArduboyAvrOledCsPort = 'D';
static constexpr uint8_t kArduboyAvrOledCsBit = 6;
static constexpr uint8_t kArduboyAvrOledDcBit = 4;
static constexpr uint8_t kArduboyAvrOledResetBit = 7;

static bool gArduboyAvrFxCsUserOverride = false;

#ifdef ENABLE_OPENCV
static std::vector<uint8_t> gArduboyAvrFxFlash;
static uint32_t gArduboyAvrFxFlashBase = 0;
static bool gArduboyAvrFxFlashBaseAuto = false;
static uint32_t gArduboyAvrFxFlashTailBase = 0;
static std::string gArduboyAvrFxFlashPath;
static int gArduboyAvrFxTraceRemaining = 0;
#endif

enum ArduboyAvrMode {
    ARDUBOY_AVR_MODE_MENU = 0,
    ARDUBOY_AVR_MODE_GAME
};

static ArduboyAvrMode gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
static int gArduboyAvrSelected = 0;

static void ArduboyAvrStartGame(int index);
static void ArduboyAvrInitDebug(void);
static void ArduboyAvrFxAttach(void);
static void ArduboyAvrFxCsHook(struct avr_irq_t *irq, uint32_t value, void *param);
static void ArduboyAvrFxSpiHook(struct avr_irq_t *irq, uint32_t value, void *param);
static void ArduboyAvrOledPortHook(struct avr_irq_t *irq, uint32_t value, void *param);

static void ArduboyAvrSetMenuWarning(const char *warning) {
    if (gArduboyAvrMenuWarning == warning) {
        return;
    }
    gArduboyAvrMenuWarning = warning;
    gUpdateDisplay = true;
}

static void ArduboyAvrClampSelection(void) {
    if (gArduboyAvrRomCount == 0) {
        gArduboyAvrSelected = 0;
        return;
    }
    if (gArduboyAvrSelected < 0) {
        gArduboyAvrSelected = 0;
    }
    const int max_index = static_cast<int>(gArduboyAvrRomCount) - 1;
    if (gArduboyAvrSelected > max_index) {
        gArduboyAvrSelected = max_index;
    }
}

static void ArduboyAvrInitDebug(void) {
#ifdef ENABLE_OPENCV
    if (gArduboyAvrDebugLevel > 0) {
        return;
    }
    const char *env = getenv("UVE5_ARDUBOY_AVR_LOG");
    if (!env) {
        return;
    }
    if (env[0] == '0' || env[0] == '\0') {
        return;
    }
    const int level = atoi(env);
    gArduboyAvrDebugLevel = (level <= 0) ? 1 : level;

    const char *log_path = getenv("UVE5_ARDUBOY_AVR_LOGFILE");
    if (!log_path || log_path[0] == '\0') {
        log_path = "arduboy_avr.log";
    }
    gArduboyAvrLogFile = fopen(log_path, "wb");
    if (gArduboyAvrLogFile) {
        // When running with verbose logging, line-buffer so logs remain visible even if the app hangs.
        const int mode = (gArduboyAvrDebugLevel >= 2) ? _IOLBF : _IOFBF;
        setvbuf(gArduboyAvrLogFile, NULL, mode, 256 * 1024);
        fprintf(gArduboyAvrLogFile, "[arduboy_avr] logging enabled level=%d\n", gArduboyAvrDebugLevel);
        fflush(gArduboyAvrLogFile);
    }
#endif
}

static bool ArduboyAvrFxParseCs(char *out_port, uint8_t *out_pin) {
    if (!out_port || !out_pin) {
        return false;
    }
#ifdef ENABLE_OPENCV
    const char *env = getenv("UVE5_ARDUBOY_AVR_FX_CS");
    if (!env || env[0] == '\0') {
        return false;
    }
    const char port = env[0];
    if (!((port >= 'A' && port <= 'Z') || (port >= 'a' && port <= 'z'))) {
        return false;
    }
    const int pin = atoi(env + 1);
    if (pin < 0 || pin > 7) {
        return false;
    }
    *out_port = (port >= 'a' && port <= 'z') ? (char)(port - 'a' + 'A') : port;
    *out_pin = (uint8_t)pin;
    return true;
#else
    return false;
#endif
}

static bool ArduboyAvrFxLooksLikeCommand(uint8_t mosi) {
    switch (mosi) {
        case 0x9F: // JEDEC ID
        case 0xAB: // signature / release from deep power-down
        case 0x90: // legacy manufacturer/device ID
        case 0x03: // READ
        case 0x0B: // FAST READ
        case 0x05: // RDSR
        case 0x06: // WREN
        case 0x04: // WRDI
        case 0xB9: // Deep power-down
            return true;
        default:
            return false;
    }
}

static bool ArduboyAvrFxLooksLikeProbe(uint8_t mosi) {
    switch (mosi) {
        case 0x9F: // JEDEC ID
        case 0xAB: // signature / release from deep power-down
        case 0x90: // legacy manufacturer/device ID
        case 0xB9: // deep power-down
            return true;
        default:
            return false;
    }
}

#ifdef ENABLE_OPENCV
namespace {
static constexpr uint8_t kArduboyAvrFxJedecManufacturer = 0xEF;
static constexpr uint8_t kArduboyAvrFxJedecMemoryType = 0x40;
static constexpr uint8_t kArduboyAvrFxJedecCapacity = 0x18;
static constexpr uint8_t kArduboyAvrFxSignature = kArduboyAvrFxJedecCapacity;
static constexpr uint8_t kArduboyAvrFxLegacyDeviceId = kArduboyAvrFxJedecCapacity;

enum ArduboyAvrFxState {
    ARDUBOY_AVR_FX_STATE_IDLE = 0,
    ARDUBOY_AVR_FX_STATE_JEDEC_ID,
    ARDUBOY_AVR_FX_STATE_SIGNATURE_ADDR,
    ARDUBOY_AVR_FX_STATE_SIGNATURE_DATA,
    ARDUBOY_AVR_FX_STATE_LEGACY_ID_ADDR,
    ARDUBOY_AVR_FX_STATE_LEGACY_ID_DATA,
    ARDUBOY_AVR_FX_STATE_READ_ADDR,
    ARDUBOY_AVR_FX_STATE_READ_DATA,
    ARDUBOY_AVR_FX_STATE_FAST_READ_ADDR,
    ARDUBOY_AVR_FX_STATE_FAST_READ_DUMMY,
    ARDUBOY_AVR_FX_STATE_FAST_READ_DATA,
    ARDUBOY_AVR_FX_STATE_RDSR,
};

static ArduboyAvrFxState gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_IDLE;
static uint32_t gArduboyAvrFxAddr = 0;
static uint8_t gArduboyAvrFxAddrBytes = 0;
static uint8_t gArduboyAvrFxOutIndex = 0;
static bool gArduboyAvrFxDeepPowerDown = false;
static bool gArduboyAvrFxLoggedFirstRead = false;
static bool gArduboyAvrFxLoggedFirstSelect = false;
static bool gArduboyAvrFxLoggedCsMismatch = false;

static void ArduboyAvrFxResetState(void) {
    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_IDLE;
    gArduboyAvrFxAddr = 0;
    gArduboyAvrFxAddrBytes = 0;
    gArduboyAvrFxOutIndex = 0;
}

static void ArduboyAvrFxResetSession(void) {
    ArduboyAvrFxResetState();
    gArduboyAvrFxLoggedFirstRead = false;
    gArduboyAvrFxLoggedFirstSelect = false;
    gArduboyAvrFxLoggedCsMismatch = false;
    gArduboyAvrFxTraceRemaining = 0;
}

static uint8_t ArduboyAvrFxRead(uint32_t addr) {
    if (addr < gArduboyAvrFxFlashBase) {
        return 0xFF;
    }
    const uint32_t off = addr - gArduboyAvrFxFlashBase;
    if (off < gArduboyAvrFxFlash.size()) {
        return gArduboyAvrFxFlash[off];
    }
    return 0xFF;
}

static bool ArduboyAvrFxIsExcludedCsCandidate(char port, uint8_t bit) {
    if (port == kArduboyAvrOledCsPort) {
        if (bit == kArduboyAvrOledCsBit || bit == kArduboyAvrOledDcBit || bit == kArduboyAvrOledResetBit) {
            return true;
        }
    }
    // Exclude SPI SCK/MOSI pins on ATmega32u4 (PORTB1/2); these can idle low and confuse CS detection.
    if (port == 'B' && (bit == 1 || bit == 2)) {
        return true;
    }
    return false;
}

static void ArduboyAvrFxConfigureCs(char port, uint8_t bit, const char *reason) {
    if (!gArduboyAvr) {
        return;
    }
    if (port == gArduboyAvrFxCsPort && bit == gArduboyAvrFxCsBit) {
        return;
    }

    gArduboyAvrFxCsPort = port;
    gArduboyAvrFxCsBit = bit;

    avr_irq_t *cs_irq = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ(port), IOPORT_IRQ_REG_PORT);
    if (cs_irq) {
        avr_irq_register_notify(cs_irq, ArduboyAvrFxCsHook, reinterpret_cast<void *>(static_cast<uintptr_t>(port)));
    }

    avr_ioport_state_t st = {};
    if (avr_ioctl(gArduboyAvr, AVR_IOCTL_IOPORT_GETSTATE(port), &st) == 0) {
        gArduboyAvrFxCsLevel = static_cast<uint8_t>((st.port >> bit) & 1U);
        gArduboyAvrFxSelected = (gArduboyAvrFxCsLevel == 0);
    } else {
        gArduboyAvrFxCsLevel = 1;
        gArduboyAvrFxSelected = false;
    }

    ArduboyAvrFxResetState();
    gArduboyAvrFxLoggedFirstSelect = false;
    gArduboyAvrFxLoggedCsMismatch = false;

    ArduboyAvrLog(1, "[arduboy_avr][fx] CS set to %c%u (%s)\n",
                  port,
                  static_cast<unsigned>(bit),
                  reason ? reason : "unknown");
}

static bool ArduboyAvrFxMaybeAutoDetectCs(uint8_t mosi) {
    if (gArduboyAvrFxCsUserOverride) {
        return false;
    }
    if (!gArduboyAvr || gArduboyAvrMode != ARDUBOY_AVR_MODE_GAME) {
        return false;
    }
    if (gArduboyAvrOledSelected) {
        return false;
    }
    if (!ArduboyAvrFxLooksLikeProbe(mosi)) {
        return false;
    }

    struct Candidate {
        char port;
        uint8_t bit;
        int score;
    };

    static int scores[5][8] = {};
    static unsigned samples = 0;

    const char ports[] = { 'B', 'C', 'D', 'E', 'F' };
    for (size_t pi = 0; pi < sizeof(ports); ++pi) {
        avr_ioport_state_t st = {};
        if (avr_ioctl(gArduboyAvr, AVR_IOCTL_IOPORT_GETSTATE(ports[pi]), &st) != 0) {
            continue;
        }
        const uint8_t low_outputs = static_cast<uint8_t>(st.ddr & static_cast<uint8_t>(~st.port));
        for (uint8_t b = 0; b < 8; ++b) {
            if ((low_outputs >> b) & 1U) {
                if (ArduboyAvrFxIsExcludedCsCandidate(ports[pi], b)) {
                    continue;
                }
                scores[pi][b]++;
            }
        }
    }
    samples++;

    Candidate best = { 0, 0, -1 };
    Candidate second = { 0, 0, -1 };
    for (size_t pi = 0; pi < sizeof(ports); ++pi) {
        for (uint8_t b = 0; b < 8; ++b) {
            const int s = scores[pi][b];
            if (s > best.score) {
                second = best;
                best = { ports[pi], b, s };
            } else if (s > second.score) {
                second = { ports[pi], b, s };
            }
        }
    }

    // Require a few samples and a clear winner before switching.
    if (samples >= 3 && best.score > second.score && best.score >= 2) {
        ArduboyAvrFxConfigureCs(best.port, best.bit, "auto-detect");
        return true;
    }
    return false;
}

static const char *ArduboyAvrFxStateName(void) {
    switch (gArduboyAvrFxState) {
        case ARDUBOY_AVR_FX_STATE_IDLE:
            return "IDLE";
        case ARDUBOY_AVR_FX_STATE_JEDEC_ID:
            return "JEDEC";
        case ARDUBOY_AVR_FX_STATE_SIGNATURE_ADDR:
            return "SIG_ADDR";
        case ARDUBOY_AVR_FX_STATE_SIGNATURE_DATA:
            return "SIG_DATA";
        case ARDUBOY_AVR_FX_STATE_LEGACY_ID_ADDR:
            return "LEG_ADDR";
        case ARDUBOY_AVR_FX_STATE_LEGACY_ID_DATA:
            return "LEG_DATA";
        case ARDUBOY_AVR_FX_STATE_READ_ADDR:
            return "RD_ADDR";
        case ARDUBOY_AVR_FX_STATE_READ_DATA:
            return "RD_DATA";
        case ARDUBOY_AVR_FX_STATE_FAST_READ_ADDR:
            return "FR_ADDR";
        case ARDUBOY_AVR_FX_STATE_FAST_READ_DUMMY:
            return "FR_DUMMY";
        case ARDUBOY_AVR_FX_STATE_FAST_READ_DATA:
            return "FR_DATA";
        case ARDUBOY_AVR_FX_STATE_RDSR:
            return "RDSR";
        default:
            return "?";
    }
}

static void ArduboyAvrFxFinalizeFlashBase(void) {
    if (!gArduboyAvrFxFlashBaseAuto) {
        return;
    }
    // If the ROM reads from a low address, the image is likely a "head" segment (base=0).
    // Otherwise we keep the default tail mapping and lock it in.
    if (gArduboyAvrFxAddr < gArduboyAvrFxFlash.size()) {
        gArduboyAvrFxFlashBase = 0;
        ArduboyAvrLog(1, "[arduboy_avr][fx] auto-selected base=head (0x000000)\n");
    } else {
        ArduboyAvrLog(1,
                      "[arduboy_avr][fx] auto-selected base=tail (0x%06lX)\n",
                      static_cast<unsigned long>(gArduboyAvrFxFlashTailBase));
    }
    gArduboyAvrFxFlashBaseAuto = false;
}

static uint8_t ArduboyAvrFxOnByte(uint8_t mosi) {
    if (!gArduboyAvrFxReady) {
        return 0xFF;
    }

    if (gArduboyAvrFxDeepPowerDown && mosi != 0xAB) {
        return 0xFF;
    }

    switch (gArduboyAvrFxState) {
        case ARDUBOY_AVR_FX_STATE_IDLE: {
            switch (mosi) {
                case 0x9F: // JEDEC ID
                    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_JEDEC_ID;
                    gArduboyAvrFxOutIndex = 0;
                    return 0xFF;
                case 0xAB: // signature / release from deep power-down
                    gArduboyAvrFxDeepPowerDown = false;
                    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_SIGNATURE_ADDR;
                    gArduboyAvrFxAddrBytes = 0;
                    gArduboyAvrFxOutIndex = 0;
                    return kArduboyAvrFxSignature;
                case 0x90: // legacy manufacturer/device ID
                    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_LEGACY_ID_ADDR;
                    gArduboyAvrFxAddrBytes = 0;
                    gArduboyAvrFxOutIndex = 0;
                    return 0xFF;
                case 0x03: // READ
                    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_READ_ADDR;
                    gArduboyAvrFxAddr = 0;
                    gArduboyAvrFxAddrBytes = 0;
                    return 0xFF;
                case 0x0B: // FAST READ
                    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_FAST_READ_ADDR;
                    gArduboyAvrFxAddr = 0;
                    gArduboyAvrFxAddrBytes = 0;
                    return 0xFF;
                case 0x05: // RDSR
                    gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_RDSR;
                    return 0xFF;
                case 0xB9: // deep power-down
                    gArduboyAvrFxDeepPowerDown = true;
                    return 0xFF;
                case 0x06: // WREN
                case 0x04: // WRDI
                default:
                    return 0xFF;
            }
        }
        case ARDUBOY_AVR_FX_STATE_JEDEC_ID: {
            const uint8_t id[] = {
                kArduboyAvrFxJedecManufacturer,
                kArduboyAvrFxJedecMemoryType,
                kArduboyAvrFxJedecCapacity,
            };
            uint8_t resp = (gArduboyAvrFxOutIndex < sizeof(id)) ? id[gArduboyAvrFxOutIndex] : 0xFF;
            gArduboyAvrFxOutIndex++;
            return resp;
        }
        case ARDUBOY_AVR_FX_STATE_SIGNATURE_ADDR: {
            gArduboyAvrFxAddrBytes++;
            if (gArduboyAvrFxAddrBytes >= 3) {
                gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_SIGNATURE_DATA;
                gArduboyAvrFxOutIndex = 0;
            }
            return kArduboyAvrFxSignature;
        }
        case ARDUBOY_AVR_FX_STATE_SIGNATURE_DATA: {
            gArduboyAvrFxOutIndex++;
            return kArduboyAvrFxSignature;
        }
        case ARDUBOY_AVR_FX_STATE_LEGACY_ID_ADDR: {
            gArduboyAvrFxAddrBytes++;
            if (gArduboyAvrFxAddrBytes >= 3) {
                gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_LEGACY_ID_DATA;
                gArduboyAvrFxOutIndex = 0;
            }
            return 0xFF;
        }
        case ARDUBOY_AVR_FX_STATE_LEGACY_ID_DATA: {
            const uint8_t id[] = {
                kArduboyAvrFxJedecManufacturer,
                kArduboyAvrFxLegacyDeviceId,
            };
            uint8_t resp = (gArduboyAvrFxOutIndex < sizeof(id)) ? id[gArduboyAvrFxOutIndex] : 0xFF;
            gArduboyAvrFxOutIndex++;
            return resp;
        }
        case ARDUBOY_AVR_FX_STATE_READ_ADDR: {
            gArduboyAvrFxAddr = (gArduboyAvrFxAddr << 8) | mosi;
            gArduboyAvrFxAddrBytes++;
            if (gArduboyAvrFxAddrBytes >= 3) {
                ArduboyAvrFxFinalizeFlashBase();
                gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_READ_DATA;
            }
            return 0xFF;
        }
        case ARDUBOY_AVR_FX_STATE_READ_DATA: {
            if (!gArduboyAvrFxLoggedFirstRead) {
                gArduboyAvrFxLoggedFirstRead = true;
                ArduboyAvrLog(1,
                              "[arduboy_avr][fx] first READ addr=0x%06lX file='%s' size=%zu\n",
                              static_cast<unsigned long>(gArduboyAvrFxAddr),
                              gArduboyAvrFxFlashPath.c_str(),
                              gArduboyAvrFxFlash.size());
            }
            uint8_t resp = ArduboyAvrFxRead(gArduboyAvrFxAddr);
            gArduboyAvrFxAddr++;
            return resp;
        }
        case ARDUBOY_AVR_FX_STATE_FAST_READ_ADDR: {
            gArduboyAvrFxAddr = (gArduboyAvrFxAddr << 8) | mosi;
            gArduboyAvrFxAddrBytes++;
            if (gArduboyAvrFxAddrBytes >= 3) {
                ArduboyAvrFxFinalizeFlashBase();
                gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_FAST_READ_DUMMY;
            }
            return 0xFF;
        }
        case ARDUBOY_AVR_FX_STATE_FAST_READ_DUMMY:
            gArduboyAvrFxState = ARDUBOY_AVR_FX_STATE_FAST_READ_DATA;
            return 0xFF;
        case ARDUBOY_AVR_FX_STATE_FAST_READ_DATA: {
            if (!gArduboyAvrFxLoggedFirstRead) {
                gArduboyAvrFxLoggedFirstRead = true;
                ArduboyAvrLog(1,
                              "[arduboy_avr][fx] first FAST_READ addr=0x%06lX file='%s' size=%zu\n",
                              static_cast<unsigned long>(gArduboyAvrFxAddr),
                              gArduboyAvrFxFlashPath.c_str(),
                              gArduboyAvrFxFlash.size());
            }
            uint8_t resp = ArduboyAvrFxRead(gArduboyAvrFxAddr);
            gArduboyAvrFxAddr++;
            return resp;
        }
        case ARDUBOY_AVR_FX_STATE_RDSR:
            return 0x00;
        default:
            return 0xFF;
    }
}

static bool ArduboyAvrFxLoadFlash(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    static constexpr uint32_t kFxTotalSize = 0x1000000; // 16 MiB (W25Q128)
    if (size > static_cast<std::streamsize>(kFxTotalSize)) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
    if (!file) {
        return false;
    }

    uint32_t base = 0;
    bool auto_base = false;
    uint32_t tail_base = 0;
    const char *base_env = getenv("UVE5_ARDUBOY_AVR_FX_BASE");
    if (base_env && base_env[0] != '\0') {
        char *end = nullptr;
        const unsigned long parsed = strtoul(base_env, &end, 0);
        if (!end || *end != '\0') {
            return false;
        }
        base = static_cast<uint32_t>(parsed);
        if (static_cast<uint64_t>(base) + static_cast<uint64_t>(data.size()) > kFxTotalSize) {
            return false;
        }
    } else if (data.size() < kFxTotalSize) {
        // Many FX images are trimmed "tail segments" of a full 16 MiB flashcart image,
        // but some are head segments (base=0). Default to tail, then auto-correct on
        // the first READ/FAST_READ address if it clearly targets the head.
        tail_base = static_cast<uint32_t>(kFxTotalSize - data.size());
        base = tail_base;
        auto_base = true;
    }

    gArduboyAvrFxFlash.swap(data);
    gArduboyAvrFxFlashBase = base;
    gArduboyAvrFxFlashBaseAuto = auto_base;
    gArduboyAvrFxFlashTailBase = tail_base;
    gArduboyAvrFxFlashPath = path;
    gArduboyAvrFxReady = true;
    ArduboyAvrFxResetSession();
    ArduboyAvrLog(1,
                  "[arduboy_avr][fx] loaded FX image '%s' (%lld bytes) base=0x%06lX%s\n",
                  path,
                  static_cast<long long>(size),
                  static_cast<unsigned long>(gArduboyAvrFxFlashBase),
                  gArduboyAvrFxFlashBaseAuto ? " (auto)" : "");
    return true;
}

static bool ArduboyAvrFxTryAutoLoadForRom(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }
    const char *env = getenv("UVE5_ARDUBOY_AVR_FX_FLASH");
    if (env && env[0] != '\0') {
        return ArduboyAvrFxLoadFlash(env);
    }

    const std::string basename = std::string(name) + ".bin";
    const std::string candidates[] = {
        std::string("game/") + basename,
        basename,
        std::string("src/app/opencv/game/") + basename,
    };
    for (const auto &path : candidates) {
        if (ArduboyAvrFxLoadFlash(path.c_str())) {
            ArduboyAvrLog(1, "[arduboy_avr][fx] auto-loaded FX image for ROM '%s' from '%s'\n",
                          name,
                          path.c_str());
            return true;
        }
    }

    ArduboyAvrLog(1,
                  "[arduboy_avr][fx] ROM '%s' needs FX data: provide UVE5_ARDUBOY_AVR_FX_FLASH or place '%s'\n",
                  name,
                  basename.c_str());
    return false;
}
} // namespace
#endif

static void ArduboyAvrFxCsHook(struct avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq;
    const char port =
        param ? static_cast<char>(reinterpret_cast<uintptr_t>(param)) : (gArduboyAvrFxCsPort ? gArduboyAvrFxCsPort : '?');
    if (port != gArduboyAvrFxCsPort) {
        return;
    }
    const uint8_t port_value = static_cast<uint8_t>(value & 0xFF);
    const uint8_t bit = (gArduboyAvrFxCsBit < 8) ? gArduboyAvrFxCsBit : 0;
    const uint8_t cs_level = static_cast<uint8_t>((port_value >> bit) & 1U);
    if (cs_level == gArduboyAvrFxCsLevel) {
        return;
    }
    gArduboyAvrFxCsLevel = cs_level;
    gArduboyAvrFxSelected = (cs_level == 0);
#ifdef ENABLE_OPENCV
    if (gArduboyAvrFxSelected && !gArduboyAvrFxLoggedFirstSelect && gArduboyAvrDebugLevel >= 1) {
        gArduboyAvrFxLoggedFirstSelect = true;
        ArduboyAvrLog(1,
                      "[arduboy_avr][fx] CS asserted (%c%u)\n",
                      gArduboyAvrFxCsPort ? gArduboyAvrFxCsPort : '?',
                      static_cast<unsigned>(bit));
    }
    if (gArduboyAvrDebugLevel >= 2) {
        gArduboyAvrFxTraceRemaining = gArduboyAvrFxSelected ? 80 : 0;
    }
#endif
    ArduboyAvrLog(2,
                  "[arduboy_avr][fx] CS(bit=%u)=%u PORT=0x%02X\n",
                  static_cast<unsigned>(bit),
                  static_cast<unsigned>(cs_level),
                  static_cast<unsigned>(port_value));
#ifdef ENABLE_OPENCV
    ArduboyAvrFxResetState();
#endif
}

static void ArduboyAvrFxSpiHook(struct avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq;
    (void)param;
    const uint8_t mosi = static_cast<uint8_t>(value);
    uint8_t resp = 0xFF;

#ifdef ENABLE_OPENCV
    const bool is_probe = ArduboyAvrFxLooksLikeProbe(mosi);
    if (!gArduboyAvrOledSelected && !gArduboyAvrFxSelected && is_probe) {
        (void)ArduboyAvrFxMaybeAutoDetectCs(mosi);
    }

    if (!gArduboyAvrFxLoggedCsMismatch && gArduboyAvrDebugLevel >= 1 && !gArduboyAvrOledSelected &&
        !gArduboyAvrFxSelected && is_probe) {
        gArduboyAvrFxLoggedCsMismatch = true;
        if (gArduboyAvrFxCsUserOverride) {
            ArduboyAvrLog(1,
                          "[arduboy_avr][fx] saw FX probe 0x%02X but FX CS isn't asserted; check UVE5_ARDUBOY_AVR_FX_CS\n",
                          static_cast<unsigned>(mosi));
        } else {
            ArduboyAvrLog(1,
                          "[arduboy_avr][fx] saw FX probe 0x%02X but FX CS isn't asserted; trying auto-detect\n",
                          static_cast<unsigned>(mosi));
        }
    }

    // Arduboy FX flash shares SPI with the OLED, but the OLED is write-only and does not drive MISO.
    // Respond only when the flash CS is active; otherwise MISO floats high (0xFF).
    if (!gArduboyAvrOledSelected && gArduboyAvrFxReady && gArduboyAvrFxSelected) {
        const char *state_before = ArduboyAvrFxStateName();
        const uint32_t addr_before = gArduboyAvrFxAddr;
        resp = ArduboyAvrFxOnByte(mosi);
        if (gArduboyAvrDebugLevel >= 2 && gArduboyAvrFxTraceRemaining > 0) {
            gArduboyAvrFxTraceRemaining--;
            ArduboyAvrLog(2,
                          "[arduboy_avr][fx] trace pc=0x%04X MOSI=0x%02X MISO=0x%02X st=%s->%s addr=0x%06lX->0x%06lX\n",
                          gArduboyAvr ? static_cast<unsigned>(gArduboyAvr->pc) : 0U,
                          static_cast<unsigned>(mosi),
                          static_cast<unsigned>(resp),
                          state_before,
                          ArduboyAvrFxStateName(),
                          static_cast<unsigned long>(addr_before),
                          static_cast<unsigned long>(gArduboyAvrFxAddr));
        }
    }

    // Don't force-exit on missing FX image: many hex-only games may still
    // contain SPI-looking constants, and CS detection can be wrong.
#endif

    if (gArduboyAvrDebugLevel >= 2 && !gArduboyAvrOledSelected && gArduboyAvrFxSelected &&
        ArduboyAvrFxLooksLikeCommand(mosi)) {
        ArduboyAvrLog(2,
                      "[arduboy_avr][fx] sniff MOSI=0x%02X fx_sel=%d fx_ready=%d oled_sel=%d\n",
                      static_cast<unsigned>(mosi),
                      gArduboyAvrFxSelected ? 1 : 0,
                      gArduboyAvrFxReady ? 1 : 0,
                      gArduboyAvrOledSelected ? 1 : 0);
    }

    if (gArduboyAvrSpiIn) {
        avr_raise_irq(gArduboyAvrSpiIn, resp);
    }
}

static void ArduboyAvrOledPortHook(struct avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq;
    (void)param;
    const uint8_t port_value = static_cast<uint8_t>(value & 0xFF);
    const uint8_t cs_level = static_cast<uint8_t>((port_value >> kArduboyAvrOledCsBit) & 1U);
    gArduboyAvrOledSelected = (cs_level == 0);
    if (cs_level != gArduboyAvrOledCsLevel) {
        gArduboyAvrOledCsLevel = cs_level;
        ArduboyAvrLog(2,
                      "[arduboy_avr][oled] CS(bit=%u)=%u PORT%c=0x%02X\n",
                      static_cast<unsigned>(kArduboyAvrOledCsBit),
                      static_cast<unsigned>(cs_level),
                      kArduboyAvrOledCsPort,
                      static_cast<unsigned>(port_value));
    }
}

static void ArduboyAvrFxAttach(void) {
    if (!gArduboyAvr) {
        return;
    }

    gArduboyAvrSpiOut = avr_io_getirq(gArduboyAvr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT);
    gArduboyAvrSpiIn = avr_io_getirq(gArduboyAvr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT);

    // Default chosen to match Arduboy FX wiring (PORTD bit1).
    // Can be overridden via UVE5_ARDUBOY_AVR_FX_CS (e.g. D1, B7, C7, ...).
    char cs_port = 'D';
    uint8_t cs_pin = 1;
    gArduboyAvrFxCsUserOverride = ArduboyAvrFxParseCs(&cs_port, &cs_pin);
    gArduboyAvrFxCsBit = cs_pin;
    gArduboyAvrFxCsPort = cs_port;

    gArduboyAvrFxCs = avr_io_getirq(gArduboyAvr,
                                    AVR_IOCTL_IOPORT_GETIRQ(cs_port),
                                    IOPORT_IRQ_REG_PORT);

    if (gArduboyAvrSpiOut) {
        avr_irq_register_notify(gArduboyAvrSpiOut, ArduboyAvrFxSpiHook, NULL);
    }
    if (gArduboyAvrFxCs) {
        avr_irq_register_notify(gArduboyAvrFxCs, ArduboyAvrFxCsHook,
                                reinterpret_cast<void *>(static_cast<uintptr_t>(cs_port)));
    }

    avr_ioport_state_t st = {};
    if (avr_ioctl(gArduboyAvr, AVR_IOCTL_IOPORT_GETSTATE(cs_port), &st) == 0) {
        const uint8_t bit = (gArduboyAvrFxCsBit < 8) ? gArduboyAvrFxCsBit : 0;
        gArduboyAvrFxCsLevel = static_cast<uint8_t>((st.port >> bit) & 1U);
        gArduboyAvrFxSelected = (gArduboyAvrFxCsLevel == 0);
    }

    ArduboyAvrLog(1,
                  "[arduboy_avr][fx] attach: CS=%c%u via=PORT present=%d\n",
                  cs_port,
                  static_cast<unsigned>(cs_pin),
                  gArduboyAvrFxCs ? 1 : 0);
#ifdef ENABLE_OPENCV
    ArduboyAvrFxResetSession();
#endif
}

static void ArduboyAvrSleep(avr_t *avr, avr_cycle_count_t howLong) {
    (void)avr;
    (void)howLong;
}

static const char *ArduboyAvrCpuStateName(int state) {
    switch (state) {
        case cpu_Limbo:
            return "LIMBO";
        case cpu_Stopped:
            return "STOP";
        case cpu_Running:
            return "RUN";
        case cpu_Sleeping:
            return "SLEEP";
        case cpu_Step:
            return "STEP";
        case cpu_StepDone:
            return "STEP_DONE";
        case cpu_Done:
            return "DONE";
        case cpu_Crashed:
            return "CRASH";
        default:
            return "?";
    }
}

static uint8_t ArduboyAvrReverseBits(uint8_t v) {
    v = static_cast<uint8_t>((v >> 4) | (v << 4));
    v = static_cast<uint8_t>(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = static_cast<uint8_t>(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

static void ArduboyAvrUpdateButtons(void) {
    if (!gArduboyAvr) {
        return;
    }

    // Buttons are active-low with pull-ups.
    const uint32_t left = (gArduboyAvrButtons & LEFT_BUTTON) ? 0U : 1U;   // PF5
    const uint32_t right = (gArduboyAvrButtons & RIGHT_BUTTON) ? 0U : 1U; // PF6
    const uint32_t up = (gArduboyAvrButtons & UP_BUTTON) ? 0U : 1U;       // PF7
    const uint32_t down = (gArduboyAvrButtons & DOWN_BUTTON) ? 0U : 1U;   // PF4
    const uint32_t a = (gArduboyAvrButtons & A_BUTTON) ? 0U : 1U;         // PE6
    const uint32_t b = (gArduboyAvrButtons & B_BUTTON) ? 0U : 1U;         // PB4

    // Use per-pin IRQs to avoid clobbering unrelated port bits (e.g. SPI MISO on PORTB).
    if (gButtonPortF4) {
        avr_raise_irq(gButtonPortF4, down);
    }
    if (gButtonPortF5) {
        avr_raise_irq(gButtonPortF5, left);
    }
    if (gButtonPortF6) {
        avr_raise_irq(gButtonPortF6, right);
    }
    if (gButtonPortF7) {
        avr_raise_irq(gButtonPortF7, up);
    }
    if (gButtonPortE6) {
        avr_raise_irq(gButtonPortE6, a);
    }
    if (gButtonPortB4) {
        avr_raise_irq(gButtonPortB4, b);
    }
}

static bool ArduboyAvrLoadHex(avr_t *avr, const char *hex) {
    if (!avr || !hex) {
        return false;
    }

    uint32_t segment = 0;
    const char *line = hex;
    char line_buf[560];
    uint8_t bline[272];

    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t len = eol ? static_cast<size_t>(eol - line) : strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            len--;
        }
        if (len == 0) {
            line = eol ? eol + 1 : line + len;
            continue;
        }
        if (line[0] != ':') {
            line = eol ? eol + 1 : line + len;
            continue;
        }
        if (len - 1 >= sizeof(line_buf)) {
            return false;
        }
        memcpy(line_buf, line + 1, len - 1);
        line_buf[len - 1] = '\0';

        const int hexlen = read_hex_string(line_buf, bline, sizeof(bline));
        if (hexlen < 5) {
            return false;
        }

        uint8_t chk = 0;
        for (int i = 0; i < hexlen - 1; ++i) {
            chk = static_cast<uint8_t>(chk + bline[i]);
        }
        chk = static_cast<uint8_t>(0x100 - chk);
        if (chk != bline[hexlen - 1]) {
            return false;
        }

        const uint8_t count = bline[0];
        const uint16_t addr = static_cast<uint16_t>(bline[1] << 8) | bline[2];
        const uint8_t type = bline[3];
        if (hexlen < static_cast<int>(count + 5)) {
            return false;
        }

        switch (type) {
            case 0x00: {
                if (count > 0) {
                    const uint32_t load_addr = segment | addr;
                    avr_loadcode(avr, bline + 4, count, load_addr);
                }
                break;
            }
            case 0x01:
                return true;
            case 0x02: {
                if (count >= 2) {
                    segment = (static_cast<uint32_t>(bline[4]) << 8 | bline[5]) << 4;
                }
                break;
            }
            case 0x04: {
                if (count >= 2) {
                    segment = (static_cast<uint32_t>(bline[4]) << 8 | bline[5]) << 16;
                }
                break;
            }
            default:
                break;
        }

        line = eol ? eol + 1 : line + len;
    }
    return true;
}

static bool ArduboyAvrInit(void) {
    ArduboyAvrInitDebug();
    if (gArduboyAvrInitialized) {
        return true;
    }

    gArduboyAvr = avr_make_mcu_by_name("atmega32u4");
    if (!gArduboyAvr) {
        return false;
    }
    if (avr_init(gArduboyAvr) != 0) {
        free(gArduboyAvr);
        gArduboyAvr = NULL;
        return false;
    }

    gArduboyAvr->frequency = 16000000;
    gArduboyAvr->sleep = ArduboyAvrSleep;
    gArduboyAvr->log = LOG_ERROR;

    ssd1306_init(gArduboyAvr, &gArduboyDisplay, SSD1306_VIRT_COLUMNS,
                 SSD1306_VIRT_PAGES * 8);
    ssd1306_wiring_t wiring = {
        .chip_select = { 'D', 6 },
        .data_instruction = { 'D', 4 },
        .reset = { 'D', 7 },
    };
    ssd1306_connect(&gArduboyDisplay, &wiring);

    // Track OLED CS so we can distinguish OLED traffic from FX flash probes.
    gArduboyAvrOledPortD = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ(kArduboyAvrOledCsPort),
                                        IOPORT_IRQ_REG_PORT);
    if (gArduboyAvrOledPortD) {
        avr_irq_register_notify(gArduboyAvrOledPortD, ArduboyAvrOledPortHook, NULL);
    }

    gButtonPortB4 = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('B'),
                                  IOPORT_IRQ_PIN4);
    gButtonPortE6 = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('E'),
                                  IOPORT_IRQ_PIN6);
    gButtonPortF4 = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('F'),
                                  IOPORT_IRQ_PIN4);
    gButtonPortF5 = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('F'),
                                  IOPORT_IRQ_PIN5);
    gButtonPortF6 = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('F'),
                                  IOPORT_IRQ_PIN6);
    gButtonPortF7 = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('F'),
                                  IOPORT_IRQ_PIN7);
    if (!gButtonPortB4 || !gButtonPortE6 || !gButtonPortF4 || !gButtonPortF5 ||
        !gButtonPortF6 || !gButtonPortF7) {
        avr_terminate(gArduboyAvr);
        free(gArduboyAvr);
        gArduboyAvr = NULL;
        return false;
    }

    ArduboyAvrFxAttach();

    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    avr_reset(gArduboyAvr);
    gArduboyAvr->run_cycle_limit = 4000;
    ssd1306_set_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY, 1);

    gArduboyAvrInitialized = true;
    return true;
}

void ARDUBOY_AVR_Enter(void) {
    if (!ArduboyAvrInit()) {
        return;
    }

    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
    ArduboyAvrClampSelection();
    ArduboyAvrSetMenuWarning(NULL);

    gArduboyAvrHasFrame = false;
    gArduboyAvrFrameReady = false;
    gRequestDisplayScreen = DISPLAY_ARDUBOY_AVR;
    GUI_SelectNextDisplay(DISPLAY_ARDUBOY_AVR);
    gRequestDisplayScreen = DISPLAY_INVALID;
    gUpdateDisplay = true;

#ifdef ENABLE_OPENCV
    // Optional: auto-start a ROM for debugging (name or index).
    const char *auto_start = getenv("UVE5_ARDUBOY_AVR_AUTOSTART");
    if (auto_start && auto_start[0] != '\0' && auto_start[0] != '0') {
        int index = -1;
        char *end = nullptr;
        const long parsed = strtol(auto_start, &end, 10);
        if (end && *end == '\0') {
            index = static_cast<int>(parsed);
        } else {
            for (unsigned int i = 0; i < gArduboyAvrRomCount; ++i) {
                const char *name = gArduboyAvrRoms[i].name;
                if (name && strcmp(name, auto_start) == 0) {
                    index = static_cast<int>(i);
                    break;
                }
            }
        }
        if (index >= 0 && index < static_cast<int>(gArduboyAvrRomCount)) {
            gArduboyAvrSelected = index;
            ArduboyAvrClampSelection();
            ArduboyAvrStartGame(index);
        } else {
            ArduboyAvrLog(1, "[arduboy_avr][rom] invalid AUTOSTART '%s'\n", auto_start);
        }
    }
#endif
}

void ARDUBOY_AVR_ExitToMain(void) {
    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
    ArduboyAvrSetMenuWarning(NULL);
    gRequestDisplayScreen = DISPLAY_MAIN;
    gUpdateDisplay = true;
}

void ARDUBOY_AVR_TimeSlice10ms(void) {
    if (!gArduboyAvrInitialized) {
        return;
    }
    if (gArduboyAvrMode != ARDUBOY_AVR_MODE_GAME) {
        return;
    }

    const avr_cycle_count_t target =
        gArduboyAvr->cycle + avr_usec_to_cycles(gArduboyAvr, 10000);

    while ((gArduboyAvr->state == cpu_Running || gArduboyAvr->state == cpu_Sleeping) &&
           gArduboyAvr->cycle < target &&
           gArduboyAvrMode == ARDUBOY_AVR_MODE_GAME) {
        avr_run(gArduboyAvr);
    }

    if (gArduboyAvrDebugLevel >= 2 && gArduboyAvrLastCpuState != gArduboyAvr->state) {
        gArduboyAvrLastCpuState = gArduboyAvr->state;
        ArduboyAvrLog(2,
                      "[arduboy_avr][cpu] state=%s pc=0x%04X sregI=%u cycle=%llu\n",
                      ArduboyAvrCpuStateName(gArduboyAvr->state),
                      static_cast<unsigned>(gArduboyAvr->pc),
                      static_cast<unsigned>(gArduboyAvr->sreg[S_I] ? 1 : 0),
                      static_cast<unsigned long long>(gArduboyAvr->cycle));
    }

    if (gArduboyAvr->state == cpu_Done || gArduboyAvr->state == cpu_Crashed) {
        ArduboyAvrLog(1,
                      "[arduboy_avr][cpu] halted state=%s; returning to menu\n",
                      ArduboyAvrCpuStateName(gArduboyAvr->state));
        ArduboyAvrSetMenuWarning(kArduboyAvrCpuHaltWarning);
        gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
        gUpdateDisplay = true;
        return;
    }

    if (ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY)) {
        if (!gArduboyAvrFirstFrameLogged && gArduboyAvrDebugLevel >= 1) {
            gArduboyAvrFirstFrameLogged = true;
            ArduboyAvrLog(1, "[arduboy_avr][frame] first OLED frame dirty\n");
        }
        gArduboyAvrFrameReady = true;
        gUpdateDisplay = true;
    }
}

static uint8_t ArduboyAvrMapKey(KEY_Code_t key) {
    switch (key) {
        case KEY_2:
            return UP_BUTTON;
        case KEY_8:
            return DOWN_BUTTON;
        case KEY_4:
            return LEFT_BUTTON;
        case KEY_6:
            return RIGHT_BUTTON;
        case KEY_UP:
            return A_BUTTON;
        case KEY_DOWN:
            return B_BUTTON;
        default:
            return 0;
    }
}

void ARDUBOY_AVR_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (Key == KEY_EXIT && !bKeyPressed) {
        // In-game EXIT returns to the ROM list; EXIT on the list returns to the main UI.
        if (gArduboyAvrMode == ARDUBOY_AVR_MODE_GAME) {
            gArduboyAvrButtons = 0;
            ArduboyAvrUpdateButtons();
            gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
            ArduboyAvrSetMenuWarning(NULL);
            gUpdateDisplay = true;
        } else {
            ARDUBOY_AVR_ExitToMain();
        }
        return;
    }

    if (gArduboyAvrMode == ARDUBOY_AVR_MODE_MENU) {
        if (!bKeyPressed || bKeyHeld) {
            return;
        }
        if (Key == KEY_2) {
            gArduboyAvrSelected--;
            ArduboyAvrClampSelection();
            ArduboyAvrSetMenuWarning(NULL);
            gUpdateDisplay = true;
            return;
        }
        if (Key == KEY_8) {
            gArduboyAvrSelected++;
            ArduboyAvrClampSelection();
            ArduboyAvrSetMenuWarning(NULL);
            gUpdateDisplay = true;
            return;
        }
        if (Key == KEY_UP) {
            ArduboyAvrStartGame(gArduboyAvrSelected);
            return;
        }
        return;
    }

    const uint8_t mask = ArduboyAvrMapKey(Key);
    if (mask == 0) {
        return;
    }

    if (bKeyPressed) {
        gArduboyAvrButtons = static_cast<uint8_t>(gArduboyAvrButtons | mask);
    } else {
        gArduboyAvrButtons = static_cast<uint8_t>(gArduboyAvrButtons & ~mask);
    }
    ArduboyAvrUpdateButtons();

    (void)bKeyHeld;
}

static void ArduboyAvrRenderBlank(void) {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    for (int i = 0; i < FRAME_LINES; ++i) {
        memset(gFrameBuffer[i], 0, sizeof(gFrameBuffer[i]));
    }
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

static void ArduboyAvrRenderMenu(void) {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    UI_DisplayClear();

    if (gArduboyAvrRomCount == 0) {
        UI_PrintStringSmall("Arduboy AVR", 0, 127, 0);
        UI_PrintStringSmall("No ROMs", 0, 127, 2);
        ST7565_BlitStatusLine();
        ST7565_BlitFullScreen();
        return;
    }

    const int total = static_cast<int>(gArduboyAvrRomCount);
    const int window = 5;
    int start = 0;
    if (total > window) {
        start = gArduboyAvrSelected - window / 2;
        if (start < 0) {
            start = 0;
        }
        if (start > total - window) {
            start = total - window;
        }
    }

    const int lines = (total < window) ? total : window;
    const bool has_prev = (start > 0);
    const bool has_next = (start + lines < total);

    // Header with scroll indicators.
    UI_PrintStringSmall("Arduboy AVR", 8, 119, 0);
    if (has_prev) {
        UI_PrintStringSmall("^", 0, 0, 0);
    }
    if (has_next) {
        UI_PrintStringSmall("v", LCD_WIDTH - 7, 0, 0);
    }

    for (int i = 0; i < lines; ++i) {
        const int index = start + i;
        char line[32];
        const char *name = gArduboyAvrRoms[index].name ? gArduboyAvrRoms[index].name : "(null)";
        const bool needs_fx = gArduboyAvrRoms[index].needs_fx;
        if (needs_fx) {
            snprintf(line, sizeof(line), "%s (FX)", name);
        } else {
            snprintf(line, sizeof(line), "%s", name);
        }
        const uint8_t row = static_cast<uint8_t>(i + 1);
        UI_PrintStringSmall(line, 0, 127, row);
        if (index == gArduboyAvrSelected) {
            for (uint8_t x = 0; x < LCD_WIDTH; ++x) {
                gFrameBuffer[row][x] ^= 0xFF;
            }
        }
    }

    if (gArduboyAvrMenuWarning) {
        UI_PrintStringSmall(gArduboyAvrMenuWarning, 0, 127, 6);
    }
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

static void ArduboyAvrStartGame(int index) {
    if (gArduboyAvrRomCount == 0) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(gArduboyAvrRomCount)) {
        return;
    }
    ArduboyAvrLog(1,
                  "[arduboy_avr][rom] start index=%d name='%s' needs_fx=%d fx_ready=%d\n",
                  index,
                  gArduboyAvrRoms[index].name ? gArduboyAvrRoms[index].name : "(null)",
                  gArduboyAvrRoms[index].needs_fx ? 1 : 0,
                  gArduboyAvrFxReady ? 1 : 0);
    if (!ArduboyAvrInit()) {
        return;
    }
#ifdef ENABLE_OPENCV
    // Reset FX state between game launches.
    gArduboyAvrFxSelected = (gArduboyAvrFxCsLevel == 0);
    gArduboyAvrFxReady = false;
    gArduboyAvrFxFlash.clear();
    gArduboyAvrFxFlashBase = 0;
    gArduboyAvrFxFlashBaseAuto = false;
    gArduboyAvrFxFlashTailBase = 0;
    gArduboyAvrFxFlashPath.clear();
    gArduboyAvrFxDeepPowerDown = false;
    ArduboyAvrFxResetSession();

    if (gArduboyAvrRoms[index].needs_fx) {
        if (!ArduboyAvrFxTryAutoLoadForRom(gArduboyAvrRoms[index].name)) {
            ArduboyAvrLog(1,
                          "[arduboy_avr][fx] continuing without FX image for ROM '%s'\n",
                          gArduboyAvrRoms[index].name ? gArduboyAvrRoms[index].name : "(null)");
            ArduboyAvrSetMenuWarning(kArduboyAvrFxWarning);
        }
    }
#else
    if (gArduboyAvrRoms[index].needs_fx) {
        ArduboyAvrSetMenuWarning(kArduboyAvrFxWarning);
        gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
        return;
    }
#endif

    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    if (!ArduboyAvrLoadHex(gArduboyAvr, gArduboyAvrRoms[index].hex)) {
        return;
    }

    avr_reset(gArduboyAvr);
    gArduboyAvr->run_cycle_limit = 4000;
    ssd1306_set_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY, 1);

    gArduboyAvrMode = ARDUBOY_AVR_MODE_GAME;
    gArduboyAvrHasFrame = false;
    gArduboyAvrFrameReady = false;
    gArduboyAvrFirstFrameLogged = false;
    gArduboyAvrLastCpuState = -1;
    gUpdateDisplay = true;

    ArduboyAvrLog(1,
                  "[arduboy_avr][rom] entered GAME mode name='%s'\n",
                  gArduboyAvrRoms[index].name ? gArduboyAvrRoms[index].name : "(null)");
}

static void ArduboyAvrRenderDisplay(void) {
    const bool display_on = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_DISPLAY_ON);
    if (!display_on) {
        ArduboyAvrRenderBlank();
        return;
    }

    const bool flip_x = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_SEGMENT_REMAP_0);
    const bool flip_y = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_COM_SCAN_NORMAL);
    const bool invert = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_DISPLAY_INVERTED);

    for (int page = 0; page < SSD1306_VIRT_PAGES; ++page) {
        const int src_page = flip_y ? (SSD1306_VIRT_PAGES - 1 - page) : page;
        uint8_t *dest = (page == 0) ? gStatusLine : gFrameBuffer[page - 1];
        for (int col = 0; col < SSD1306_VIRT_COLUMNS; ++col) {
            const int src_col = flip_x ? (SSD1306_VIRT_COLUMNS - 1 - col) : col;
            uint8_t v = gArduboyDisplay.vram[src_page][src_col];
            if (flip_y) {
                v = ArduboyAvrReverseBits(v);
            }
            if (invert) {
                v = static_cast<uint8_t>(~v);
            }
            dest[col] = v;
        }
    }

    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

void ARDUBOY_AVR_Render(void) {
    if (!gArduboyAvrInitialized) {
        return;
    }

    if (gArduboyAvrMode == ARDUBOY_AVR_MODE_MENU) {
        ArduboyAvrRenderMenu();
        return;
    }

    if (!gArduboyAvrFrameReady) {
        if (!gArduboyAvrHasFrame) {
            ArduboyAvrRenderBlank();
            gArduboyAvrHasFrame = true;
        }
        return;
    }

    ArduboyAvrRenderDisplay();
    gArduboyAvrFrameReady = false;
    gArduboyAvrHasFrame = true;
    ssd1306_set_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY, 0);
}
