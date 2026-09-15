#include "bq25672.h"

/* ---- */
/*  LSB values per datasheet Rev.B - VERIFY against your revision.     */
/* ---- */
#define BQ25672_VBAT_LSB_MV         1u
#define BQ25672_VSYS_LSB_MV         1u
#define BQ25672_VBUS_LSB_MV         1u
#define BQ25672_IBAT_LSB_MA         1u
#define BQ25672_IBUS_LSB_MA         1u

#define BQ25672_CHRG_V_LSB_MV       10u
#define BQ25672_CHRG_I_LSB_MA       10u
#define BQ25672_IN_I_LSB_MA         10u

#define BQ25672_CHRG_V_MIN_MV       3000u
#define BQ25672_CHRG_V_MAX_MV       18800u
#define BQ25672_CHRG_I_MAX_MA       3000u
#define BQ25672_IN_I_MAX_MA         3300u

/* ---- */
/*  Limits for new setters                    */
/* ---- */

/* VSYSMIN (REG 0x00 bits 5:0): 250 mV/LSB, offset 2500 mV          */
#define BQ25672_VSYSMIN_LSB_MV      250u
#define BQ25672_VSYSMIN_OFFSET_MV   2500u
#define BQ25672_VSYSMIN_MAX_MV      16000u
#define BQ25672_VSYSMIN_MASK        0x3Fu   /* bits 5:0 */

/* VINDPM (REG 0x05): 100 mV/LSB, offset 0 mV, 8-bit             */
#define BQ25672_VINDPM_LSB_MV       100u
#define BQ25672_VINDPM_OFFSET_MV    0u
#define BQ25672_VINDPM_MAX_MV       22000u

/* IPRECHG (REG 0x08 bits 5:0): 40 mA/LSB, offset 0 mA, 4-bit        */
#define BQ25672_IPRECHG_LSB_MA      40u
#define BQ25672_IPRECHG_OFFSET_MA   0u
#define BQ25672_IPRECHG_MAX_MA      2000u
#define BQ25672_IPRECHG_MASK        0x3Fu   /* bits 5:0 */

/* ITERM (REG 0x09 bits 4:0): 40 mA/LSB, offset 0 mA, 5-bit          */
#define BQ25672_ITERM_LSB_MA        40u
#define BQ25672_ITERM_OFFSET_MA     0u
#define BQ25672_ITERM_MAX_MA        1000u
#define BQ25672_ITERM_MASK          0x1Fu   /* bits 4:0 */

/* ---- */
/*  Temperature ADC constants                    */
/* ---- */

/* TDIE_ADC (REG 0x41): signed 16-bit, 1 raw unit = 0.5 °C          */
/* Whole-degree result: raw / 2 (truncated, ±0.5 °C error)          */

/* TS_ADC (REG 0x3F): unsigned 16-bit, LSB ≈ 0.0977 % of VREGN.
 * Conversion to whole percent: pct = (raw * 977) / 10000            */
#define BQ25672_TS_SCALE_NUM        977u
#define BQ25672_TS_SCALE_DEN        10000u

/* ================================================================== */
/*  HAL wrappers                                                        */
/* ================================================================== */
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

/* ================================================================== */
/*  Low-level access                                                    */
/* ================================================================== */
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

/* ================================================================== */
/*  Init                                                                */
/* ================================================================== */
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

/* ================================================================== */
/*  Charging control                                                    */
/* ================================================================== */
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

/* ================================================================== */
/*  [Group 3] Additional charger settings                              */
/* ================================================================== */

Bq25672Status bq25672SetMinSysVoltageMv(Bq25672Handle *dev, uint16_t mv)
{
    uint8_t code;
    if (mv < BQ25672_VSYSMIN_OFFSET_MV || mv > BQ25672_VSYSMIN_MAX_MV)
        return BQ25672_ERR_PARAM;
    code = (uint8_t)((mv - BQ25672_VSYSMIN_OFFSET_MV) / BQ25672_VSYSMIN_LSB_MV);
    return bq25672UpdateBits(dev, BQ25672_REG_MIN_SYS_V,
        BQ25672_VSYSMIN_MASK, code);
}

Bq25672Status bq25672SetInputVoltageMv(Bq25672Handle *dev, uint16_t mv)
{
    uint8_t code;
    if (mv < BQ25672_VINDPM_OFFSET_MV || mv > BQ25672_VINDPM_MAX_MV)
        return BQ25672_ERR_PARAM;
    code = (uint8_t)((mv - BQ25672_VINDPM_OFFSET_MV) / BQ25672_VINDPM_LSB_MV);
    return bq25672WriteReg(dev, BQ25672_REG_IN_V_LIM, code);
}

Bq25672Status bq25672SetPrechargeCurrentMa(Bq25672Handle *dev, uint16_t ma)
{
    uint8_t code;
    if (ma < BQ25672_IPRECHG_OFFSET_MA || ma > BQ25672_IPRECHG_MAX_MA)
        return BQ25672_ERR_PARAM;
    code = (uint8_t)((ma - BQ25672_IPRECHG_OFFSET_MA) / BQ25672_IPRECHG_LSB_MA);
    return bq25672UpdateBits(dev, BQ25672_REG_PRECHG_CTRL,
        BQ25672_IPRECHG_MASK, code);
}

Bq25672Status bq25672SetTermCurrentMa(Bq25672Handle *dev, uint16_t ma)
{
    uint8_t code;
    if (ma < BQ25672_ITERM_OFFSET_MA || ma > BQ25672_ITERM_MAX_MA)
        return BQ25672_ERR_PARAM;
    code = (uint8_t)((ma - BQ25672_ITERM_OFFSET_MA) / BQ25672_ITERM_LSB_MA);
    return bq25672UpdateBits(dev, BQ25672_REG_TERM_CTRL,
        BQ25672_ITERM_MASK, code);
}

Bq25672Status bq25672SetSafetyTimer(Bq25672Handle *dev, bool enable,
    Bq25672ChgTimer duration)
{
    uint8_t mask, val;
    if ((uint8_t)duration > 3u) return BQ25672_ERR_PARAM;
    mask = (uint8_t)(BQ25672_CHRG_TIMER_EN_BIT | BQ25672_CHRG_TIMER_SEL_MASK);
    val  = enable
        ? (uint8_t)(BQ25672_CHRG_TIMER_EN_BIT |
              (((uint8_t)duration << 1) & (uint8_t)BQ25672_CHRG_TIMER_SEL_MASK))
        : 0u;
    return bq25672UpdateBits(dev, BQ25672_REG_CHRG_TIMER, mask, val);
}

/* ================================================================== */
/*  Charge status                                                       */
/* ================================================================== */
Bq25672Status bq25672GetChargeState(Bq25672Handle *dev,
    Bq25672ChargeState *state)
{
    uint8_t reg;
    Bq25672Status st;
    if (!state) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg(dev, BQ25672_REG_CHRG_STATUS1, &reg);
    if (st != BQ25672_OK) return st;
    *state = (Bq25672ChargeState)((reg >> 5) & 0x07u);
    return BQ25672_OK;
}

/* ================================================================== */
/*  [Group 2] Status and faults                                        */
/* ================================================================== */

Bq25672Status bq25672GetStatus0(Bq25672Handle *dev, Bq25672ChargeStatus0 *st0)
{
    uint8_t reg;
    Bq25672Status st;
    if (!dev || !st0) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg(dev, BQ25672_REG_CHRG_STATUS0, &reg);
    if (st != BQ25672_OK) return st;

    st0->vbusPresent = (reg & BQ25672_FLAG0_VBUS_PRESENT) != 0u;
    st0->ac1Present  = (reg & BQ25672_FLAG0_AC1_PRESENT)  != 0u;
    st0->ac2Present  = (reg & BQ25672_FLAG0_AC2_PRESENT)  != 0u;
    st0->powerGood   = (reg & BQ25672_FLAG0_PG)           != 0u;
    st0->poorSource  = (reg & BQ25672_FLAG0_POORSRC)      != 0u;
    st0->watchdog    = (reg & BQ25672_FLAG0_WD)           != 0u;
    st0->vindpm      = (reg & BQ25672_FLAG0_VINDPM)       != 0u;
    st0->iindpm      = (reg & BQ25672_FLAG0_IINDPM)       != 0u;
    return BQ25672_OK;
}

Bq25672Status bq25672GetFaultStatus(Bq25672Handle *dev,
    Bq25672FaultStatus *fault)
{
    uint8_t reg;
    Bq25672Status st;
    if (!dev || !fault) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg(dev, BQ25672_REG_FAULT_STATUS0, &reg);
    if (st != BQ25672_OK) return st;

    fault->vac1Ovp = (reg & BQ25672_FAULT0_VAC1_OVP) != 0u;
    fault->vac2Ovp = (reg & BQ25672_FAULT0_VAC2_OVP) != 0u;
    fault->convOcp = (reg & BQ25672_FAULT0_CONV_OCP)  != 0u;
    fault->ibatOcp = (reg & BQ25672_FAULT0_IBAT_OCP)  != 0u;
    fault->ibusOcp = (reg & BQ25672_FAULT0_IBUS_OCP)  != 0u;
    fault->vbatOvp = (reg & BQ25672_FAULT0_VBAT_OVP)  != 0u;
    fault->vbusOvp = (reg & BQ25672_FAULT0_VBUS_OVP)  != 0u;
    fault->ibatReg = (reg & BQ25672_FAULT0_IBAT_REG)  != 0u;
    return BQ25672_OK;
}

/* ================================================================== */
/*  ADC                                                                 */
/* ================================================================== */
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

/* ================================================================== */
/*  [Group 1] Temperature                                              */
/* ================================================================== */

Bq25672Status bq25672ReadTdieDegC(Bq25672Handle *dev, int16_t *degC)
{
    uint16_t raw;
    Bq25672Status st;
    if (!dev || !degC) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_TDIE_ADC, &raw);
    if (st != BQ25672_OK) return st;
    /* Signed 16-bit, 1 LSB = 0.5 °C → divide by 2 for whole °C.    */
    *degC = (int16_t)((int16_t)raw / 2);
    return BQ25672_OK;
}

Bq25672Status bq25672ReadTsRatioPct(Bq25672Handle *dev, uint16_t *pct)
{
    uint16_t raw;
    Bq25672Status st;
    if (!dev || !pct) return BQ25672_ERR_PARAM;
    st = bq25672ReadReg16(dev, BQ25672_REG_TS_ADC, &raw);
    if (st != BQ25672_OK) return st;
    /* 1 LSB ≈ 0.0981 % → whole percent: (raw * 981) / 10000.        */
    *pct = (uint16_t)(((uint32_t)raw * BQ25672_TS_SCALE_NUM) /
        BQ25672_TS_SCALE_DEN);
    return BQ25672_OK;
}

/* ================================================================== */
/*  [Group 4] All-in-one ADC snapshot                                  */
/* ================================================================== */

Bq25672Status bq25672ReadSnapshot(Bq25672Handle *dev, Bq25672Snapshot *snap)
{
    Bq25672Status st;
    if (!dev || !snap) return BQ25672_ERR_PARAM;

    if ((st = bq25672ReadVbusMv(dev,    &snap->vbusMv))     != BQ25672_OK) return st;
    if ((st = bq25672ReadVbatMv(dev,    &snap->vbatMv))     != BQ25672_OK) return st;
    if ((st = bq25672ReadVsysMv(dev,    &snap->vsysMv))     != BQ25672_OK) return st;
    if ((st = bq25672ReadIbatMa(dev,    &snap->ibatMa))     != BQ25672_OK) return st;
    if ((st = bq25672ReadIbusMa(dev,    &snap->ibusMa))     != BQ25672_OK) return st;
    if ((st = bq25672ReadTdieDegC(dev,  &snap->tdieDegC))   != BQ25672_OK) return st;
    if ((st = bq25672ReadTsRatioPct(dev,&snap->tsRatioPct)) != BQ25672_OK) return st;

    return BQ25672_OK;
}

/* ================================================================== */
/*  INT pin API                                                         */
/* ================================================================== */

Bq25672Status bq25672SetIntCallback(Bq25672Handle *dev,
    Bq25672IntCallback cb, void *ctx)
{
    if (!dev) return BQ25672_ERR_PARAM;
    dev->intCallback    = cb;
    dev->intCallbackCtx = ctx;
    return BQ25672_OK;
}

Bq25672Status bq25672ConfigureChargeMask(Bq25672Handle *dev, uint8_t mask0)
{
    if (!dev) return BQ25672_ERR_PARAM;
    return bq25672WriteReg(dev, BQ25672_REG_CHRG_MASK0, mask0);
}

Bq25672Status bq25672ConfigureFaultMask(Bq25672Handle *dev, uint8_t mask0)
{
    if (!dev) return BQ25672_ERR_PARAM;
    return bq25672WriteReg(dev, BQ25672_REG_FAULT_MASK0, mask0);
}

Bq25672Status bq25672ReadAndClearFlags(Bq25672Handle *dev,
    Bq25672IntEvent *event)
{
    uint8_t f0, f1, ff0;
    Bq25672Status st;

    if (!dev || !event) return BQ25672_ERR_PARAM;

    /* --- Read latched flags ---------------------------------------- */
    if ((st = bq25672ReadReg(dev, BQ25672_REG_CHRG_FLAG0,  &f0))  != BQ25672_OK) return st;
    if ((st = bq25672ReadReg(dev, BQ25672_REG_CHRG_FLAG1,  &f1))  != BQ25672_OK) return st;
    if ((st = bq25672ReadReg(dev, BQ25672_REG_FAULT_FLAG0, &ff0)) != BQ25672_OK) return st;

    /* --- Clear flags (W1C): write back the same bytes --------------- */
    if ((st = bq25672WriteReg(dev, BQ25672_REG_CHRG_FLAG0,  f0))  != BQ25672_OK) return st;
    if ((st = bq25672WriteReg(dev, BQ25672_REG_CHRG_FLAG1,  f1))  != BQ25672_OK) return st;
    if ((st = bq25672WriteReg(dev, BQ25672_REG_FAULT_FLAG0, ff0)) != BQ25672_OK) return st;

    /* --- Decode CHRG_FLAG0 ----------------------------------------- */
    event->charge.vbusPresent   = (f0 & BQ25672_FLAG0_VBUS_PRESENT) != 0u;
    event->charge.ac1Present    = (f0 & BQ25672_FLAG0_AC1_PRESENT)  != 0u;
    event->charge.ac2Present    = (f0 & BQ25672_FLAG0_AC2_PRESENT)  != 0u;
    event->charge.powerGood     = (f0 & BQ25672_FLAG0_PG)           != 0u;
    event->charge.poorSource    = (f0 & BQ25672_FLAG0_POORSRC)      != 0u;
    event->charge.watchdog      = (f0 & BQ25672_FLAG0_WD)           != 0u;
    event->charge.vindpm        = (f0 & BQ25672_FLAG0_VINDPM)       != 0u;
    event->charge.iindpm        = (f0 & BQ25672_FLAG0_IINDPM)       != 0u;

    /* --- Decode CHRG_FLAG1 ----------------------------------------- */
    event->charge.tReg          = (f1 & BQ25672_FLAG1_TREG)        != 0u;
    event->charge.chgStatChange = (f1 & BQ25672_FLAG1_CHG_STAT)    != 0u;

    /* --- Decode CHRG_FLAG2 ----------------------------------------- */
    event->charge.chgTimerExp   = (f1 & BQ25672_FLAG2_CHG_TMR)     != 0u;
    event->charge.adcDone       = (f1 & BQ25672_FLAG2_ADC_DONE)    != 0u;

    /* --- Decode FAULT_FLAG0 ---------------------------------------- */
    event->fault.vac1Ovp = (ff0 & BQ25672_FAULT0_VAC1_OVP) != 0u;
    event->fault.vac2Ovp = (ff0 & BQ25672_FAULT0_VAC2_OVP) != 0u;
    event->fault.convOcp = (ff0 & BQ25672_FAULT0_CONV_OCP)  != 0u;
    event->fault.ibatOcp = (ff0 & BQ25672_FAULT0_IBAT_OCP)  != 0u;
    event->fault.ibusOcp = (ff0 & BQ25672_FAULT0_IBUS_OCP)  != 0u;
    event->fault.vbatOvp = (ff0 & BQ25672_FAULT0_VBAT_OVP)  != 0u;
    event->fault.vbusOvp = (ff0 & BQ25672_FAULT0_VBUS_OVP)  != 0u;
    event->fault.ibatReg = (ff0 & BQ25672_FAULT0_IBAT_REG)  != 0u;

    return BQ25672_OK;
}

Bq25672Status bq25672HandleInt(Bq25672Handle *dev)
{
    Bq25672IntEvent event;
    Bq25672Status   st;

    if (!dev) return BQ25672_ERR_PARAM;

    st = bq25672ReadAndClearFlags(dev, &event);
    if (st != BQ25672_OK) return st;

    if (dev->intCallback)
        dev->intCallback(&event, dev->intCallbackCtx);

    return BQ25672_OK;
}
