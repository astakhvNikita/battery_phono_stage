#ifndef BQ25672_H
#define BQ25672_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  I2C 7-bit address                                                  */
/* ------------------------------------------------------------------ */
#define BQ25672_I2C_ADDR            0x6B

/* ------------------------------------------------------------------ */
/*  Register map (datasheet Rev.B, section 8.5)                        */
/* ------------------------------------------------------------------ */
#define BQ25672_REG_MIN_SYS_V       0x00
#define BQ25672_REG_CHRG_V_LIM      0x01
#define BQ25672_REG_CHRG_I_LIM      0x03
#define BQ25672_REG_IN_V_LIM        0x05
#define BQ25672_REG_IN_I_LIM        0x06
#define BQ25672_REG_CHRG_CTRL0      0x0F
#define BQ25672_REG_CHRG_CTRL1      0x10
#define BQ25672_REG_CHRG_TIMER      0x0E
#define BQ25672_REG_CHRG_STATUS0    0x1B
#define BQ25672_REG_CHRG_STATUS1    0x1C
#define BQ25672_REG_FAULT_STATUS0   0x20
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

/* ------------------------------------------------------------------ */
/*  Bit fields                                                         */
/* ------------------------------------------------------------------ */
#define BQ25672_ADC_CTRL_EN         (1u << 7)
#define BQ25672_ADC_CTRL_RATE       (1u << 6)
#define BQ25672_CHRG_CTRL0_EN_CHG   (1u << 5)

/* ------------------------------------------------------------------ */
/*  Charge state (CHG_STAT, REG 0x1C bits 7:5)                         */
/* ------------------------------------------------------------------ */
typedef enum {
    BQ25672_CHG_STATE_NOT_CHARGING = 0x0,
    BQ25672_CHG_STATE_TRICKLE      = 0x1,
    BQ25672_CHG_STATE_PRECHARGE    = 0x2,
    BQ25672_CHG_STATE_FAST_CC      = 0x3,
    BQ25672_CHG_STATE_TAPER_CV     = 0x4,
    BQ25672_CHG_STATE_TOPOFF_TMR   = 0x6,
    BQ25672_CHG_STATE_DONE         = 0x7
} Bq25672ChargeState;

/* ------------------------------------------------------------------ */
/*  Return status                                                      */
/* ------------------------------------------------------------------ */
typedef enum {
    BQ25672_OK        = 0,
    BQ25672_ERR_IO    = -1,
    BQ25672_ERR_PARAM = -2,
    BQ25672_ERR_ID    = -3
} Bq25672Status;

/* ------------------------------------------------------------------ */
/*  Hardware abstraction (HAL). All callbacks return 0 on success.     */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  Device handle                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    Bq25672Hal hal;
    bool       initialised;
} Bq25672Handle;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672Init(Bq25672Handle *dev, const Bq25672Hal *hal);

Bq25672Status bq25672ReadReg(Bq25672Handle *dev, uint8_t reg, uint8_t *val);
Bq25672Status bq25672WriteReg(Bq25672Handle *dev, uint8_t reg, uint8_t val);
Bq25672Status bq25672ReadReg16(Bq25672Handle *dev, uint8_t reg,
    uint16_t *val);
Bq25672Status bq25672UpdateBits(Bq25672Handle *dev, uint8_t reg,
    uint8_t mask, uint8_t val);

Bq25672Status bq25672EnableCharging(Bq25672Handle *dev, bool enable);
Bq25672Status bq25672SetChargeVoltageMv(Bq25672Handle *dev, uint16_t mv);
Bq25672Status bq25672SetChargeCurrentMa(Bq25672Handle *dev, uint16_t ma);
Bq25672Status bq25672SetInputCurrentMa(Bq25672Handle *dev, uint16_t ma);

Bq25672Status bq25672GetChargeState(Bq25672Handle *dev,
    Bq25672ChargeState *state);

Bq25672Status bq25672EnableAdc(Bq25672Handle *dev, bool enable, bool oneShot);
Bq25672Status bq25672ReadVbatMv(Bq25672Handle *dev, uint16_t *mv);
Bq25672Status bq25672ReadVsysMv(Bq25672Handle *dev, uint16_t *mv);
Bq25672Status bq25672ReadVbusMv(Bq25672Handle *dev, uint16_t *mv);
Bq25672Status bq25672ReadIbatMa(Bq25672Handle *dev, int16_t *ma);
Bq25672Status bq25672ReadIbusMa(Bq25672Handle *dev, uint16_t *ma);

#ifdef __cplusplus
}
#endif

#endif /* BQ25672_H */
