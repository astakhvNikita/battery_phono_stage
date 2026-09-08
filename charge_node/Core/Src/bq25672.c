#include "bq25672.h"

/* ------------------------------------------------------------------ */
/*  LSB values per datasheet Rev.B - VERIFY against your revision.     */
/* ------------------------------------------------------------------ */
#define BQ25672_VBAT_LSB_MV       1u
#define BQ25672_VSYS_LSB_MV       1u
#define BQ25672_VBUS_LSB_MV       1u
#define BQ25672_IBAT_LSB_MA       1u
#define BQ25672_IBUS_LSB_MA       1u

#define BQ25672_CHRG_V_LSB_MV     10u
#define BQ25672_CHRG_I_LSB_MA     10u
#define BQ25672_IN_I_LSB_MA       10u

#define BQ25672_CHRG_V_MIN_MV     3000u
#define BQ25672_CHRG_V_MAX_MV     18800u
#define BQ25672_CHRG_I_MAX_MA     3000u
#define BQ25672_IN_I_MAX_MA       3300u

/* ------------------------------------------------------------------ */
/*  HAL wrappers                                                       */
/* ------------------------------------------------------------------ */
static Bq25672Status bq25672HalWrite(Bq25672Handle *dev, uint8_t reg,
    const uint8_t *data, uint16_t len)
{
    if (dev->hal.i2cWrite(dev->hal.devAddr, reg, data, len,
            dev->hal.ctx) != 0)
        return BQ25672_ERR_IO;
    return BQ25672_OK;
}

static Bq25672Status bq25672HalRead(Bq25672Handle *dev, uint8_t reg,
    uint8_t *data, uint16_t len)
{
    if (dev->hal.i2cRead(dev->hal.devAddr, reg, data, len,
            dev->hal.ctx) != 0)
        return BQ25672_ERR_IO;
    return BQ25672_OK;
}

/* ------------------------------------------------------------------ */
/*  Low-level access                                                   */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672ReadReg(Bq25672Handle *dev, uint8_t reg, uint8_t *val)
{
    if (!dev || !val) return BQ25672_ERR_PARAM;
    return bq25672HalRead(dev, reg, val, 1);
}

Bq25672Status bq25672WriteReg(Bq25672Handle *dev, uint8_t reg, uint8_t val)
{
    if (!dev) return BQ25672_ERR_PARAM;
    return bq25672HalWrite(dev, reg, &val, 1);
}

Bq25672Status bq25672ReadReg16(Bq25672Handle *dev, uint8_t reg,
    uint16_t *val)
{
    uint8_t buf[2];
    Bq25672Status st;
    if (!dev || !val) return BQ25672_ERR_PARAM;
    st = bq25672HalRead(dev, reg, buf, 2);
    if (st != BQ25672_OK) return st;
    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return BQ25672_OK;
}

Bq25672Status bq25672UpdateBits(Bq25672Handle *dev, uint8_t reg,
    uint8_t mask, uint8_t val)
{
    uint8_t tmp;
    Bq25672Status st = bq25672ReadReg(dev, reg, &tmp);
    if (st != BQ25672_OK) return st;
    tmp = (uint8_t)((tmp & ~mask) | (val & mask));
    return bq25672WriteReg(dev, reg, tmp);
}

/* ------------------------------------------------------------------ */
/*  Init                                                               */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672Init(Bq25672Handle *dev, const Bq25672Hal *hal)
{
    uint8_t part;
    Bq25672Status st;

    if (!dev || !hal || !hal->i2cWrite || !hal->i2cRead)
        return BQ25672_ERR_PARAM;

    dev->hal = *hal;
    if (dev->hal.devAddr == 0)
        dev->hal.devAddr = BQ25672_I2C_ADDR;
    dev->initialised = false;

    st = bq25672ReadReg(dev, BQ25672_REG_PART_INFO, &part);
    if (st != BQ25672_OK) return st;

    if (part == 0x00 || part == 0xFF)
        return BQ25672_ERR_ID;

    dev->initialised = true;
    return BQ25672_OK;
}

/* ------------------------------------------------------------------ */
/*  Charging control                                                  */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672EnableCharging(Bq25672Handle *dev, bool enable)
{
    return bq25672UpdateBits(dev, BQ25672_REG_CHRG_CTRL0,
        BQ25672_CHRG_CTRL0_EN_CHG,
        enable ? BQ25672_CHRG_CTRL0_EN_CHG : 0);
}

Bq25672Status bq25672SetChargeVoltageMv(Bq25672Handle *dev, uint16_t mv)
{
    uint16_t code;
    uint8_t buf[2];
    if (mv < BQ25672_CHRG_V_MIN_MV || mv > BQ25672_CHRG_V_MAX_MV)
        return BQ25672_ERR_PARAM;
    code = (uint16_t)(mv / BQ25672_CHRG_V_LSB_MV);
    buf[0] = (uint8_t)(code >> 8);
    buf[1] = (uint8_t)(code & 0xFF);
    return bq25672HalWrite(dev, BQ25672_REG_CHRG_V_LIM, buf, 2);
}

Bq25672Status bq25672SetChargeCurrentMa(Bq25672Handle *dev, uint16_t ma)
{
    uint16_t code;
    uint8_t buf[2];
    if (ma > BQ25672_CHRG_I_MAX_MA) return BQ25672_ERR_PARAM;
    code = (uint16_t)(ma / BQ25672_CHRG_I_LSB_MA);
    buf[0] = (uint8_t)(code >> 8);
    buf[1] = (uint8_t)(code & 0xFF);
    return bq25672HalWrite(dev, BQ25672_REG_CHRG_I_LIM, buf, 2);
}

Bq25672Status bq25672SetInputCurrentMa(Bq25672Handle *dev, uint16_t ma)
{
    uint16_t code;
    uint8_t buf[2];
    if (ma > BQ25672_IN_I_MAX_MA) return BQ25672_ERR_PARAM;
    code = (uint16_t)(ma / BQ25672_IN_I_LSB_MA);
    buf[0] = (uint8_t)(code >> 8);
    buf[1] = (uint8_t)(code & 0xFF);
    return bq25672HalWrite(dev, BQ25672_REG_IN_I_LIM, buf, 2);
}

/* ------------------------------------------------------------------ */
/*  Charge status                                                      */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672GetChargeState(Bq25672Handle *dev,
    Bq25672ChargeState *state)
{
    uint8_t reg;
    Bq25672Status st;
    if (!state) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg(dev, BQ25672_REG_CHRG_STATUS1, &reg);
    if (st != BQ25672_OK) return st;
    *state = (Bq25672ChargeState)((reg >> 5) & 0x07);
    return BQ25672_OK;
}

/* ------------------------------------------------------------------ */
/*  ADC                                                               */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672EnableAdc(Bq25672Handle *dev, bool enable, bool oneShot)
{
    uint8_t val = 0;
    if (enable) {
        val |= BQ25672_ADC_CTRL_EN;
        if (oneShot) val |= BQ25672_ADC_CTRL_RATE;
    }
    return bq25672UpdateBits(dev, BQ25672_REG_ADC_CTRL,
        BQ25672_ADC_CTRL_EN | BQ25672_ADC_CTRL_RATE, val);
}

Bq25672Status bq25672ReadVbatMv(Bq25672Handle *dev, uint16_t *mv)
{
    uint16_t raw;
    Bq25672Status st;
    if (!mv) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_VBAT_ADC, &raw);
    if (st != BQ25672_OK) return st;
    *mv = (uint16_t)(raw * BQ25672_VBAT_LSB_MV);
    return BQ25672_OK;
}

Bq25672Status bq25672ReadVsysMv(Bq25672Handle *dev, uint16_t *mv)
{
    uint16_t raw;
    Bq25672Status st;
    if (!mv) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_VSYS_ADC, &raw);
    if (st != BQ25672_OK) return st;
    *mv = (uint16_t)(raw * BQ25672_VSYS_LSB_MV);
    return BQ25672_OK;
}

Bq25672Status bq25672ReadVbusMv(Bq25672Handle *dev, uint16_t *mv)
{
    uint16_t raw;
    Bq25672Status st;
    if (!mv) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_VBUS_ADC, &raw);
    if (st != BQ25672_OK) return st;
    *mv = (uint16_t)(raw * BQ25672_VBUS_LSB_MV);
    return BQ25672_OK;
}

Bq25672Status bq25672ReadIbatMa(Bq25672Handle *dev, int16_t *ma)
{
    uint16_t raw;
    Bq25672Status st;
    if (!ma) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_IBAT_ADC, &raw);
    if (st != BQ25672_OK) return st;
    *ma = (int16_t)((int16_t)raw * (int16_t)BQ25672_IBAT_LSB_MA);
    return BQ25672_OK;
}

Bq25672Status bq25672ReadIbusMa(Bq25672Handle *dev, uint16_t *ma)
{
    uint16_t raw;
    Bq25672Status st;
    if (!ma) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_IBUS_ADC, &raw);
    if (st != BQ25672_OK) return st;
    *ma = (uint16_t)(raw * BQ25672_IBUS_LSB_MA);
    return BQ25672_OK;
}
