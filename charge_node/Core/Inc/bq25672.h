#ifndef BQ25672_H
#define BQ25672_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- */
/*  I2C 7-bit address                    */
/* ---- */
#define BQ25672_I2C_ADDR            0x6B

/* ---- */
/*  Register map (datasheet Rev.B, section 8.5)                    */
/* ---- */
#define BQ25672_REG_MIN_SYS_V       0x00
#define BQ25672_REG_CHRG_V_LIM      0x01
#define BQ25672_REG_CHRG_I_LIM      0x03
#define BQ25672_REG_IN_V_LIM        0x05
#define BQ25672_REG_IN_I_LIM        0x06
#define BQ25672_REG_PRECHG_CTRL     0x08    /* Precharge current / VBAT_LOWV    */
#define BQ25672_REG_TERM_CTRL       0x09    /* Termination current               */
#define BQ25672_REG_CHRG_TIMER      0x0E
#define BQ25672_REG_CHRG_CTRL0      0x0F
#define BQ25672_REG_CHRG_CTRL1      0x10
#define BQ25672_REG_CHRG_STATUS0    0x1B
#define BQ25672_REG_CHRG_STATUS1    0x1C
#define BQ25672_REG_FAULT_STATUS0   0x20
/* Interrupt flag registers — latched, W1C (write 1 to clear).      */
#define BQ25672_REG_CHRG_FLAG0      0x22    /* mirrors CHRG_STATUS0 bits         */
#define BQ25672_REG_CHRG_FLAG1      0x23    /* ADC_DONE, TREG, CHG_STAT change   */
/* Interrupt mask registers — 1 = masked (INT not asserted).         */
#define BQ25672_REG_CHRG_MASK0      0x28    /* masks for CHRG_FLAG0              */
#define BQ25672_REG_FAULT_FLAG0     0x26    /* mirrors FAULT_STATUS0 bits, W1C   */
#define BQ25672_REG_FAULT_MASK0     0x2C    /* masks for FAULT_FLAG0             */
#define BQ25672_REG_ADC_CTRL        0x2E
#define BQ25672_REG_ADC_FN_DIS0     0x30
#define BQ25672_REG_IBUS_ADC        0x31
#define BQ25672_REG_IBAT_ADC        0x33
#define BQ25672_REG_VBUS_ADC        0x35
#define BQ25672_REG_VBAT_ADC        0x3B
#define BQ25672_REG_VSYS_ADC        0x3D
#define BQ25672_REG_TS_ADC          0x3F
#define BQ25672_REG_TDIE_ADC        0x41
#define BQ25672_REG_PART_INFO       0x48

/* ---- */
/*  Bit fields — ADC / charge control                    */
/* ---- */
#define BQ25672_ADC_CTRL_EN         (1u << 7)
#define BQ25672_ADC_CTRL_RATE       (1u << 6)
#define BQ25672_CHRG_CTRL0_EN_CHG   (1u << 5)

/* Bit fields — CHRG_TIMER (REG 0x0E) */
#define BQ25672_CHRG_TIMER_EN_BIT   (1u << 3)   /* 1 = safety timer enabled  */
#define BQ25672_CHRG_TIMER_SEL_MASK (3u << 1)   /* CHG_TIMER[1:0] bits       */

/* Bit fields — CHRG_STATUS0 / CHRG_FLAG0 / CHRG_MASK0 (same layout) */
#define BQ25672_FLAG0_VBUS_PRESENT  (1u << 0)
#define BQ25672_FLAG0_AC1_PRESENT   (1u << 1)
#define BQ25672_FLAG0_AC2_PRESENT   (1u << 2)
#define BQ25672_FLAG0_PG            (1u << 3)
#define BQ25672_FLAG0_POORSRC       (1u << 4)
#define BQ25672_FLAG0_WD            (1u << 5)
#define BQ25672_FLAG0_VINDPM        (1u << 6)
#define BQ25672_FLAG0_IINDPM        (1u << 7)

/* Bit fields — CHRG_FLAG1 / CHRG_MASK1 */
#define BQ25672_FLAG1_TREG          (1u << 2)   /* Thermal regulation triggered */
#define BQ25672_FLAG1_CHG_STAT      (1u << 7)   /* Charge state changed         */

/* Bit fields — CHRG_FLAG2 / CHRG_MASK2 */
#define BQ25672_FLAG2_CHG_TMR       (1u << 3)   /* Charge safety timer expired  */
#define BQ25672_FLAG2_ADC_DONE      (1u << 5)   /* ADC conversion complete      */

/* Convenience masks for bq25672ConfigureChargeMask / bq25672ConfigureFaultMask */
#define BQ25672_CHRGMASK0_ALL_ON    0x00u   /* all charge events → INT enabled  */
#define BQ25672_CHRGMASK0_ALL_OFF   0xFFu   /* all charge events masked          */
#define BQ25672_FAULTMASK0_ALL_ON   0x00u   /* all fault events → INT enabled    */
#define BQ25672_FAULTMASK0_ALL_OFF  0xFFu   /* all fault events masked           */

/* Bit fields — FAULT_STATUS0 / FAULT_FLAG0 / FAULT_MASK0 (same layout) */
#define BQ25672_FAULT0_VAC1_OVP     (1u << 0)
#define BQ25672_FAULT0_VAC2_OVP     (1u << 1)
#define BQ25672_FAULT0_CONV_OCP     (1u << 2)
#define BQ25672_FAULT0_IBAT_OCP     (1u << 3)
#define BQ25672_FAULT0_IBUS_OCP     (1u << 4)
#define BQ25672_FAULT0_VBAT_OVP     (1u << 5)
#define BQ25672_FAULT0_VBUS_OVP     (1u << 6)
#define BQ25672_FAULT0_IBAT_REG     (1u << 7)

/* ---- */
/*  Charge state (CHG_STAT, REG 0x1C bits 7:5)                    */
/* ---- */
typedef enum {
    BQ25672_CHG_STATE_NOT_CHARGING = 0x0,
    BQ25672_CHG_STATE_TRICKLE      = 0x1,
    BQ25672_CHG_STATE_PRECHARGE    = 0x2,
    BQ25672_CHG_STATE_FAST_CC      = 0x3,
    BQ25672_CHG_STATE_TAPER_CV     = 0x4,
    BQ25672_CHG_STATE_TOPOFF_TMR   = 0x6,
    BQ25672_CHG_STATE_DONE         = 0x7
} Bq25672ChargeState;

/* ---- */
/*  Safety timer duration (CHG_TIMER[1:0], REG 0x0E bits 2:1)     */
/* ---- */
typedef enum {
    BQ25672_CHG_TIMER_5H  = 0,
    BQ25672_CHG_TIMER_8H  = 1,
    BQ25672_CHG_TIMER_12H = 2,
    BQ25672_CHG_TIMER_24H = 3
} Bq25672ChgTimer;

/* ---- */
/*  Return status                    */
/* ---- */
typedef enum {
    BQ25672_OK        = 0,
    BQ25672_ERR_IO    = -1,
    BQ25672_ERR_PARAM = -2,
    BQ25672_ERR_ID    = -3
} Bq25672Status;

/* ---- */
/*  Decoded CHRG_STATUS0 (REG 0x1B)                    */
/* ---- */
typedef struct {
    bool vbusPresent;   /* VBUS_PRESENT_STAT — adapter/USB detected         */
    bool ac1Present;    /* AC1_PRESENT_STAT                                  */
    bool ac2Present;    /* AC2_PRESENT_STAT                                  */
    bool powerGood;     /* PG_STAT — output in regulation                   */
    bool poorSource;    /* POORSRC_STAT — poor input source quality          */
    bool watchdog;      /* WD_STAT — watchdog timer expired                  */
    bool vindpm;        /* VINDPM_STAT — input in VINDPM regulation          */
    bool iindpm;        /* IINDPM_STAT — input in IINDPM regulation          */
} Bq25672ChargeStatus0;

/* ---- */
/*  Decoded FAULT_STATUS0 (REG 0x20)                    */
/* ---- */
typedef struct {
    bool vac1Ovp;   /* VAC1 over-voltage protection active    */
    bool vac2Ovp;   /* VAC2 over-voltage protection active    */
    bool convOcp;   /* Converter over-current protection      */
    bool ibatOcp;   /* IBAT over-current protection           */
    bool ibusOcp;   /* IBUS over-current protection           */
    bool vbatOvp;   /* VBAT over-voltage protection active    */
    bool vbusOvp;   /* VBUS over-voltage protection active    */
    bool ibatReg;   /* IBAT regulation triggered              */
} Bq25672FaultStatus;

/* ---- */
/*  Single-call ADC snapshot                    */
/* ---- */
typedef struct {
    uint16_t vbusMv;        /* Input bus voltage, mV                        */
    uint16_t vbatMv;        /* Battery pack voltage, mV                     */
    uint16_t vsysMv;        /* System voltage, mV                           */
    int16_t  ibatMa;        /* Battery current, mA (+ charge / – discharge) */
    uint16_t ibusMa;        /* Input bus current, mA                        */
    int16_t  tdieDegC;      /* Die temperature, °C (rounded to 1°C)         */
    uint16_t tsRatioPct;    /* TS-pin / VREGN ratio, % (0–100)              */
} Bq25672Snapshot;

/* ================================================================== */
/*  Interrupt (INT pin) support                                         */
/* ================================================================== */

/* ---- */
/*  Latched charge flags from CHRG_FLAG0 / CHRG_FLAG1 (W1C).         */
/* ---- */
typedef struct {
    /* CHRG_FLAG0 — a flag is set when the corresponding STATUS0 bit  */
    /* changes (any edge). Cleared by writing 1 back.                 */
    bool vbusPresent;   /* VBUS presence change                       */
    bool ac1Present;    /* AC1 presence change                        */
    bool ac2Present;    /* AC2 presence change                        */
    bool powerGood;     /* PG status change                           */
    bool poorSource;    /* Poor-source detection change               */
    bool watchdog;      /* Watchdog timer expired                     */
    bool vindpm;        /* VINDPM entry / exit                        */
    bool iindpm;        /* IINDPM entry / exit                        */

    /* CHRG_FLAG1 — set when the corresponding event occurs. */
    bool tReg;          /* Thermal regulation triggered               */
    bool chgStatChange; /* Charge state (CHG_STAT) changed            */

    /* CHRG_FLAG2 — set when the corresponding event occurs. */
    bool chgTimerExp;   /* Charge safety timer expired                */
    bool adcDone;       /* ADC one-shot conversion complete           */
} Bq25672ChargeFlags;

/* ---- */
/*  Latched fault flags from FAULT_FLAG0 (W1C).                      */
/* ---- */
typedef struct {
    bool vac1Ovp;   /* VAC1 OVP event */
    bool vac2Ovp;   /* VAC2 OVP event */
    bool convOcp;   /* Converter OCP event */
    bool ibatOcp;   /* IBAT OCP event */
    bool ibusOcp;   /* IBUS OCP event */
    bool vbatOvp;   /* VBAT OVP event */
    bool vbusOvp;   /* VBUS OVP event */
    bool ibatReg;   /* IBAT regulation event */
} Bq25672FaultFlags;

/* ---- */
/*  Combined interrupt event — passed to the application callback.   */
/* ---- */
typedef struct {
    Bq25672ChargeFlags charge;
    Bq25672FaultFlags  fault;
} Bq25672IntEvent;

/* ---- */
/*  Application INT callback.                                         */
/*  Invoked from bq25672HandleInt() with the decoded, already-cleared */
/*  flag snapshot. Must NOT perform I2C calls; set a flag and return. */
/* ---- */
typedef void (*Bq25672IntCallback)(const Bq25672IntEvent *event, void *ctx);

/* ---- */
/*  Hardware abstraction (HAL). All callbacks return 0 on success.     */
/* ---- */
typedef struct {
    /* Write "len" bytes from "data" to register "reg". */
    int (*i2cWrite)(uint8_t devAddr, uint8_t reg,
        const uint8_t *data, uint16_t len, void *ctx);

    /* Read "len" bytes from register "reg" into "data". */
    int (*i2cRead)(uint8_t devAddr, uint8_t reg,
        uint8_t *data, uint16_t len, void *ctx);

    /* Blocking delay in milliseconds. */
    void (*delayMs)(uint32_t ms, void *ctx);

    void   *ctx;        /* user context passed to every callback */
    uint8_t devAddr;    /* I2C address (0 => BQ25672_I2C_ADDR)   */
} Bq25672Hal;

/* ---- */
/*  Device handle                    */
/* ---- */
typedef struct {
    Bq25672Hal           hal;
    bool                 initialised;
    /* INT pin support (optional — zero-init means no callback)       */
    Bq25672IntCallback   intCallback;    /* registered application handler  */
    void                *intCallbackCtx; /* opaque context forwarded to cb  */
} Bq25672Handle;

/* ================================================================== */
/*  Public API                                                          */
/* ================================================================== */

/* --- Core / low-level -------------------------------------------- */
Bq25672Status bq25672Init(Bq25672Handle *dev, const Bq25672Hal *hal);

Bq25672Status bq25672ReadReg(Bq25672Handle *dev, uint8_t reg, uint8_t *val);
Bq25672Status bq25672WriteReg(Bq25672Handle *dev, uint8_t reg, uint8_t val);
Bq25672Status bq25672ReadReg16(Bq25672Handle *dev, uint8_t reg,
    uint16_t *val);
Bq25672Status bq25672UpdateBits(Bq25672Handle *dev, uint8_t reg,
    uint8_t mask, uint8_t val);

/* --- Charging control -------------------------------------------- */
Bq25672Status bq25672EnableCharging(Bq25672Handle *dev, bool enable);
Bq25672Status bq25672SetChargeVoltageMv(Bq25672Handle *dev, uint16_t mv);
Bq25672Status bq25672SetChargeCurrentMa(Bq25672Handle *dev, uint16_t ma);
Bq25672Status bq25672SetInputCurrentMa(Bq25672Handle *dev, uint16_t ma);

/* --- Additional charger settings --------------------------------- */

/* VSYSMIN (REG 0x00 bits 5:0): minimum system voltage.
 * Range: 2500–16000 mV, step 250 mV.  */
Bq25672Status bq25672SetMinSysVoltageMv(Bq25672Handle *dev, uint16_t mv);

/* VINDPM (REG 0x05): input voltage regulation threshold.
 * Range: 3600–22000 mV, step 100 mV.  */
Bq25672Status bq25672SetInputVoltageMv(Bq25672Handle *dev, uint16_t mv);

/* IPRECHG (REG 0x08 bits 5:0): precharge current.
 * Range: 40–2000 mA, step 40 mA.  */
Bq25672Status bq25672SetPrechargeCurrentMa(Bq25672Handle *dev, uint16_t ma);

/* ITERM (REG 0x09 bits 4:0): charge termination current.
 * Range: 40–1000 mA, step 40 mA.
 * NOTE: verify bit-width against your datasheet revision.           */
Bq25672Status bq25672SetTermCurrentMa(Bq25672Handle *dev, uint16_t ma);

/* CHG_TIMER (REG 0x0E): fast-charge safety timer.
 * enable = false disables the timer (clears EN_CHG_TIMER bit).      */
Bq25672Status bq25672SetSafetyTimer(Bq25672Handle *dev, bool enable,
    Bq25672ChgTimer duration);

/* --- Charge status ----------------------------------------------- */
Bq25672Status bq25672GetChargeState(Bq25672Handle *dev,
    Bq25672ChargeState *state);

/* --- Status and faults ------------------------------------------- */

/* REG 0x1B — power-source / regulation status (decoded).            */
Bq25672Status bq25672GetStatus0(Bq25672Handle *dev,
    Bq25672ChargeStatus0 *st);

/* REG 0x20 — fault flags (decoded).                                 */
Bq25672Status bq25672GetFaultStatus(Bq25672Handle *dev,
    Bq25672FaultStatus *fault);

/* --- ADC --------------------------------------------------------- */
Bq25672Status bq25672EnableAdc(Bq25672Handle *dev, bool enable, bool oneShot);
Bq25672Status bq25672ReadVbatMv(Bq25672Handle *dev, uint16_t *mv);
Bq25672Status bq25672ReadVsysMv(Bq25672Handle *dev, uint16_t *mv);
Bq25672Status bq25672ReadVbusMv(Bq25672Handle *dev, uint16_t *mv);
Bq25672Status bq25672ReadIbatMa(Bq25672Handle *dev, int16_t *ma);
Bq25672Status bq25672ReadIbusMa(Bq25672Handle *dev, uint16_t *ma);

/* --- Temperature ------------------------------------------------- */

/* TDIE_ADC (REG 0x41): internal die temperature, whole °C.
 * ADC LSB = 0.5 °C; half-degree is truncated. Range: –40 to 150 °C. */
Bq25672Status bq25672ReadTdieDegC(Bq25672Handle *dev, int16_t *degC);

/* TS_ADC (REG 0x3F): TS-pin voltage as % of VREGN (0–100).
 * ADC LSB ≈ 0.098 %; result rounded to whole percent.
 * To convert to temperature, apply your NTC thermistor lookup table. */
Bq25672Status bq25672ReadTsRatioPct(Bq25672Handle *dev, uint16_t *pct);

/* --- All-in-one ADC snapshot -------------------------------------- */

/* Read all ADC channels in one call.
 * Returns the first I²C error encountered; on error the snapshot
 * content is partial and must not be used.                           */
Bq25672Status bq25672ReadSnapshot(Bq25672Handle *dev, Bq25672Snapshot *snap);

/* ================================================================== */
/*  INT pin API                                                         */
/* ================================================================== */

/* Register (or clear with cb=NULL) the application interrupt handler.
 * ctx is forwarded verbatim to the callback on every invocation.     */
Bq25672Status bq25672SetIntCallback(Bq25672Handle *dev,
    Bq25672IntCallback cb, void *ctx);

/* Configure charge-event interrupt mask (REG 0x1F).
 *   bit = 0 → event asserts INT  (enabled)
 *   bit = 1 → event is masked    (INT suppressed)
 * Use BQ25672_CHRGMASK0_ALL_ON  (0x00) to enable all charge events.
 * Use BQ25672_CHRGMASK0_ALL_OFF (0xFF) to suppress all.             */
Bq25672Status bq25672ConfigureChargeMask(Bq25672Handle *dev, uint8_t mask0);

/* Configure fault-event interrupt mask (REG 0x22). Same polarity.
 * Use BQ25672_FAULTMASK0_ALL_ON  (0x00) to enable all fault events. */
Bq25672Status bq25672ConfigureFaultMask(Bq25672Handle *dev, uint8_t mask0);

/* Read and atomically clear (W1C) all interrupt flag registers.
 * Populates *event with the decoded flag snapshot.
 * Call from the deferred main-loop handler, NOT from the ISR itself. */
Bq25672Status bq25672ReadAndClearFlags(Bq25672Handle *dev,
    Bq25672IntEvent *event);

/* High-level INT dispatcher — reads flags, clears them, calls cb.
 *
 * Recommended pattern:
 *
 *   // ISR — keep it minimal, no I2C:
 *   void EXTI_IRQHandler(void) { g_bqIntPending = true; }
 *
 *   // Main loop or RTOS task:
 *   if (g_bqIntPending) {
 *       g_bqIntPending = false;
 *       bq25672HandleInt(&dev);
 *   }
 */
Bq25672Status bq25672HandleInt(Bq25672Handle *dev);

#ifdef __cplusplus
}
#endif

#endif /* BQ25672_H */
