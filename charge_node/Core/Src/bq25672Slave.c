/* ------------------------------------------------------------------ */
/* bq25672Slave.c                                                     */
/* I2C-slave register-map handler, wired to the concrete bq25672.h    */
/* and bq25672Soc.h headers of this project.                          */
/* ------------------------------------------------------------------ */

#include "bq25672Slave.h"
#include <string.h>

/* ================================================================== */
/* DRIVER ADAPTER                                                      */
/* The only place coupled to the concrete driver/SoC API.             */
/* Matched to bq25672.h (Rev.B register map) and bq25672Soc.h.        */
/* ================================================================== */

/* Telemetry snapshot pulled from the BQ25672 ADC each task cycle. */
typedef struct
{
    uint16_t vbatMv;
    uint16_t vsysMv;
    uint16_t vbusMv;
    int16_t  ibatMa;    /* signed: + charge / - discharge */
    uint16_t ibusMa;
    uint16_t tsRaw;     /* raw TS_ADC word   */
    uint16_t tdieRaw;   /* raw TDIE_ADC word */
} bq25672SlaveTelemetry_t;

/* Read the full ADC telemetry set. Returns true if every read
 * returned BQ25672_OK. VBAT/VSYS/VBUS/IBAT/IBUS use the driver
 * helpers; TS/TDIE have no helper, so read the raw ADC words. */
static bool driverReadTelemetry(Bq25672Handle *dev,
        bq25672SlaveTelemetry_t *tm)
{
    bool ok = true;

    ok &= (bq25672ReadVbatMv(dev, &tm->vbatMv) == BQ25672_OK);
    ok &= (bq25672ReadVsysMv(dev, &tm->vsysMv) == BQ25672_OK);
    ok &= (bq25672ReadVbusMv(dev, &tm->vbusMv) == BQ25672_OK);
    ok &= (bq25672ReadIbatMa(dev, &tm->ibatMa) == BQ25672_OK);
    ok &= (bq25672ReadIbusMa(dev, &tm->ibusMa) == BQ25672_OK);
    ok &= (bq25672ReadReg16(dev, BQ25672_REG_TS_ADC,
            &tm->tsRaw) == BQ25672_OK);
    ok &= (bq25672ReadReg16(dev, BQ25672_REG_TDIE_ADC,
            &tm->tdieRaw) == BQ25672_OK);

    return ok;
}

/* Read CHG_STAT and map the driver enum to the register enum. */
static uint8_t driverReadChgState(Bq25672Handle *dev)
{
    Bq25672ChargeState st;

    if (bq25672GetChargeState(dev, &st) != BQ25672_OK)
    {
        return BQ_CHG_STATE_NOT_CHARGING;
    }

    switch (st)
    {
        case BQ25672_CHG_STATE_NOT_CHARGING:
            return BQ_CHG_STATE_NOT_CHARGING;
        case BQ25672_CHG_STATE_TRICKLE:
            return BQ_CHG_STATE_TRICKLE;
        case BQ25672_CHG_STATE_PRECHARGE:
            return BQ_CHG_STATE_PRECHARGE;
        case BQ25672_CHG_STATE_FAST_CC:
            return BQ_CHG_STATE_FAST_CC;
        case BQ25672_CHG_STATE_TAPER_CV:
            return BQ_CHG_STATE_TAPER_CV;
        case BQ25672_CHG_STATE_TOPOFF_TMR:
            return BQ_CHG_STATE_TOPOFF;
        case BQ25672_CHG_STATE_DONE:
            return BQ_CHG_STATE_DONE;
        default:
            return BQ_CHG_STATE_NOT_CHARGING;
    }
}

/* Read FAULT_STATUS0 (reg 0x20) and translate to the register
 * FAULTS bitfield. Bit positions follow datasheet Rev.B; adjust
 * to your silicon revision if they differ. */
static uint16_t driverReadFaults(Bq25672Handle *dev)
{
    uint16_t out = 0;
    uint8_t  raw = 0;

    if (bq25672ReadReg(dev, BQ25672_REG_FAULT_STATUS0, &raw)
            != BQ25672_OK)
    {
        return BQ_FAULT_I2C_BQ;
    }

    /* FAULT_STATUS0: [6] VBUS_OVP, [5] VBAT_OVP, [3] IBAT_OCP.
     * Map the subset exposed by the register map. */
    if (raw & (1u << 5)) { out |= BQ_FAULT_VBAT_OVP; }
    if (raw & (1u << 6)) { out |= BQ_FAULT_VBUS_OVP; }
    if (raw & (1u << 3)) { out |= BQ_FAULT_IBAT_OCP; }

    return out;
}

/* ================================================================== */
/* LITTLE-ENDIAN REGISTER ACCESSORS                                   */
/* ================================================================== */

static void regWriteU16(Bq25672Slave *self, uint8_t reg, uint16_t v)
{
    self->regs[reg]     = (uint8_t)(v & 0xFF);
    self->regs[reg + 1] = (uint8_t)(v >> 8);
}

static void regWriteU32(Bq25672Slave *self, uint8_t reg, uint32_t v)
{
    self->regs[reg]     = (uint8_t)(v & 0xFF);
    self->regs[reg + 1] = (uint8_t)((v >> 8) & 0xFF);
    self->regs[reg + 2] = (uint8_t)((v >> 16) & 0xFF);
    self->regs[reg + 3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint16_t regReadU16(const Bq25672Slave *self, uint8_t reg)
{
    return (uint16_t)self->regs[reg]
            | ((uint16_t)self->regs[reg + 1] << 8);
}

/* ================================================================== */
/* WRITE CLASSIFICATION                                               */
/* Read-only registers reject master writes to stay coherent.        */
/* ================================================================== */

static bool regIsWritable(uint8_t reg)
{
    /* Control / setpoint block. */
    if (reg >= BQ_REG_CHG_ENABLE && reg <= BQ_REG_CMD)
    {
        return true;
    }

    /* SoC configuration / persist block. */
    if (reg >= BQ_REG_DESIGN_CAP_MAH
            && reg <= (BQ_REG_SOC_OVERRIDE + 1))
    {
        return true;
    }

    return false;
}

/* ================================================================== */
/* APPLY CONTROL SETPOINTS                                            */
/* Only setpoints exposed by bq25672.h are forwarded. IN_VLIM_MV,     */
/* ITERM_MA, IPRECHG_MA, TS_PROFILE and WATCHDOG_S have no driver     */
/* setter yet; they are kept in the shadow file for future use.       */
/* ================================================================== */

static void applyControlReg(Bq25672Slave *self, uint8_t reg)
{
    switch (reg)
    {
        case BQ_REG_CHG_ENABLE:
            bq25672EnableCharging(self->dev,
                    self->regs[reg] != 0);
            break;

        case BQ_REG_CHG_CURRENT_MA:
            bq25672SetChargeCurrentMa(self->dev,
                    regReadU16(self, reg));
            break;

        case BQ_REG_CHG_VOLTAGE_MV:
            bq25672SetChargeVoltageMv(self->dev,
                    regReadU16(self, reg));
            break;

        case BQ_REG_IN_ILIM_MA:
            bq25672SetInputCurrentMa(self->dev,
                    regReadU16(self, reg));
            break;

        default:
            /* No driver setter available; value is stored only. */
            break;
    }
}

/* ================================================================== */
/* APPLY SoC CONFIGURATION                                            */
/* Field names follow bq25672Soc.h (Bq25672SocConfig).                */
/* ================================================================== */

static void applySocConfigReg(Bq25672Slave *self, uint8_t reg)
{
    switch (reg)
    {
        case BQ_REG_DESIGN_CAP_MAH:
            self->soc->cfg.fullCapacityMah =
                    regReadU16(self, reg);
            break;

        case BQ_REG_COULOMBIC_EFF:
            /* Stored as 0.1 %, driver keeps a 0..1 float. */
            self->soc->cfg.coulombicEff =
                    (float)regReadU16(self, reg) / 1000.0f;
            break;

        case BQ_REG_DISCHARGE_SRC:
            self->soc->cfg.dischargeSource =
                    (Bq25672DischargeSource)self->regs[reg];
            break;

        case BQ_REG_DISCHARGE_MA:
            self->soc->cfg.dischargeCurrentMa =
                    regReadU16(self, reg);
            break;

        case BQ_REG_REST_THRESH_MA:
            self->soc->cfg.restCurrentMa =
                    (int16_t)regReadU16(self, reg);
            break;

        case BQ_REG_REST_TIME_S:
            self->soc->cfg.restTimeMs =
                    (uint32_t)regReadU16(self, reg) * 1000u;
            break;

        case BQ_REG_SOC_OVERRIDE:
            /* Forced OCV point (cell voltage in mV). */
            bq25672SocCalibrateOcv(self->soc,
                    regReadU16(self, reg));
            break;

        default:
            break;
    }
}

/* ================================================================== */
/* COMMAND DISPATCH (register 0x4F)                                   */
/* ================================================================== */

static void execCommand(Bq25672Slave *self, uint8_t opcode)
{
    switch ((bq25672RegCmd_t)opcode)
    {
        case BQ_CMD_CLEAR_FAULTS:
            self->latchedFaults = 0;
            break;

        case BQ_CMD_RESET_SOC_FROM_OCV:
            /* Recalibrate from the last measured battery voltage. */
            bq25672SocCalibrateOcv(self->soc,
                    regReadU16(self, BQ_REG_VBAT_MV));
            break;

        case BQ_CMD_SAVE_CONFIG:
            if (self->nvm.save != NULL)
            {
                self->nvm.save(
                    &self->regs[BQ_REG_DESIGN_CAP_MAH],
                    (BQ_REG_SOC_OVERRIDE + 2)
                            - BQ_REG_DESIGN_CAP_MAH);
            }
            break;

        case BQ_CMD_MCU_REBOOT:
            if (self->event.mcuReboot != NULL)
            {
                self->event.mcuReboot();
            }
            break;

        case BQ_CMD_SET_EXT_DISCHG:
            /* Host-provided discharge current for EXTERNAL mode. */
            self->soc->cfg.dischargeCurrentMa =
                    regReadU16(self, BQ_REG_EXT_DISCHG_MA);
            break;

        /* RESET_COULOMB, LOAD_DEFAULTS, ENTER/EXIT_SHIP_MODE and
         * BQ_HARD_RESET have no driver/SoC entry point in the
         * current headers; wire them up when available. */
        case BQ_CMD_NONE:
        default:
            break;
    }

    /* The command register is self-clearing after execution. */
    self->regs[BQ_REG_CMD] = BQ_CMD_NONE;
}

/* ================================================================== */
/* STATUS / FLAGS REFRESH                                             */
/* ================================================================== */

static void refreshStatus(Bq25672Slave *self, uint8_t chgState)
{
    uint16_t flags = 0;
    Bq25672SocState socState = bq25672SocGetState(self->soc);

    self->regs[BQ_REG_SOC_STATE] = (uint8_t)socState;

    switch (socState)
    {
        case BQ25672_STATE_CHARGE:
            flags |= BQ_FLAG_CHARGING;
            break;
        case BQ25672_STATE_DISCHARGE:
            flags |= BQ_FLAG_DISCHARGING;
            break;
        case BQ25672_STATE_IDLE:
        default:
            flags |= BQ_FLAG_RESTING;
            break;
    }

    if (chgState == BQ_CHG_STATE_DONE)
    {
        flags |= BQ_FLAG_CHARGE_DONE;
    }
    if (self->soc->calibrated)
    {
        flags |= BQ_FLAG_SOC_CALIBRATED;
    }
    if (self->soc->reconnectPending)
    {
        flags |= BQ_FLAG_RECONNECT_PEND;
    }

    regWriteU16(self, BQ_REG_FLAGS, flags);
}

/* ================================================================== */
/* PUBLIC API                                                         */
/* ================================================================== */

bool bq25672SlaveInit(Bq25672Slave *self,
        Bq25672Handle *dev,
        Bq25672Soc *soc,
        const Bq25672SlaveNvmHal *nvm,
        const Bq25672SlaveEventHal *event)
{
    if (self == NULL || dev == NULL || soc == NULL)
    {
        return false;
    }

    memset(self, 0, sizeof(*self));
    self->dev = dev;
    self->soc = soc;

    if (nvm != NULL)
    {
        self->nvm = *nvm;
    }
    if (event != NULL)
    {
        self->event = *event;
    }

    /* Populate immutable identification registers. */
    self->regs[BQ_REG_WHO_AM_I]      = BQ_WHO_AM_I_VALUE;
    self->regs[BQ_REG_PROTO_VERSION] = BQ_PROTO_VERSION;

    /* Seed the config block from the live SoC configuration. */
    regWriteU16(self, BQ_REG_DESIGN_CAP_MAH,
            (uint16_t)self->soc->cfg.fullCapacityMah);
    regWriteU16(self, BQ_REG_COULOMBIC_EFF,
            (uint16_t)(self->soc->cfg.coulombicEff * 1000.0f));
    self->regs[BQ_REG_DISCHARGE_SRC] =
            (uint8_t)self->soc->cfg.dischargeSource;
    regWriteU16(self, BQ_REG_DISCHARGE_MA,
            self->soc->cfg.dischargeCurrentMa);
    regWriteU16(self, BQ_REG_REST_THRESH_MA,
            (uint16_t)self->soc->cfg.restCurrentMa);
    regWriteU16(self, BQ_REG_REST_TIME_S,
            (uint16_t)(self->soc->cfg.restTimeMs / 1000u));

    /* Restore persisted configuration on top, if available. */
    if (self->nvm.load != NULL)
    {
        self->nvm.load(&self->regs[BQ_REG_DESIGN_CAP_MAH],
                (BQ_REG_SOC_OVERRIDE + 2)
                        - BQ_REG_DESIGN_CAP_MAH);
    }

    return true;
}

void bq25672SlaveOnStart(Bq25672Slave *self)
{
    /* A fresh frame resets the register pointer expectation. */
    self->cursorSet = false;
}

bool bq25672SlaveOnWrite(Bq25672Slave *self, uint8_t byte)
{
    /* First byte of the frame selects the register. */
    if (!self->cursorSet)
    {
        if (byte >= BQ_REG_MAP_SIZE)
        {
            return false;
        }
        self->cursor    = byte;
        self->cursorSet = true;
        return true;
    }

    /* Subsequent bytes are payload written at the cursor. */
    if (self->cursor >= BQ_REG_MAP_SIZE)
    {
        return false;
    }

    if (!regIsWritable(self->cursor))
    {
        /* Silently ignore writes to read-only space. */
        self->cursor++;
        return true;
    }

    self->regs[self->cursor] = byte;

    /* Act on the write according to which block it targets. */
    if (self->cursor == BQ_REG_CMD)
    {
        execCommand(self, byte);
    }
    else if (self->cursor >= BQ_REG_CHG_ENABLE
            && self->cursor <= BQ_REG_WATCHDOG_S)
    {
        applyControlReg(self, self->cursor);
    }
    else if (self->cursor >= BQ_REG_DESIGN_CAP_MAH
            && self->cursor <= (BQ_REG_SOC_OVERRIDE + 1))
    {
        applySocConfigReg(self, self->cursor);
    }

    self->cursor++;
    return true;
}

uint8_t bq25672SlaveOnRead(Bq25672Slave *self)
{
    uint8_t value;

    if (!self->cursorSet || self->cursor >= BQ_REG_MAP_SIZE)
    {
        return 0xFF;
    }

    value = self->regs[self->cursor];
    self->cursor++;
    return value;
}

void bq25672SlaveTask(Bq25672Slave *self, uint32_t elapsedMs)
{
    bq25672SlaveTelemetry_t tm;
    uint8_t chgState;

    /* Advance uptime and heartbeat. */
    self->uptimeS += elapsedMs / 1000u;
    self->heartbeat++;
    self->regs[BQ_REG_HEARTBEAT] = self->heartbeat;
    regWriteU32(self, BQ_REG_UPTIME_S, self->uptimeS);

    /* Drive the SoC estimator once per cycle. */
    bq25672SocUpdate(self->soc, elapsedMs);

    /* Refresh telemetry snapshot from the driver. */
    if (driverReadTelemetry(self->dev, &tm))
    {
        regWriteU16(self, BQ_REG_VBAT_MV, tm.vbatMv);
        regWriteU16(self, BQ_REG_VSYS_MV, tm.vsysMv);
        regWriteU16(self, BQ_REG_VBUS_MV, tm.vbusMv);
        regWriteU16(self, BQ_REG_IBAT_MA, (uint16_t)tm.ibatMa);
        regWriteU16(self, BQ_REG_IBUS_MA, tm.ibusMa);
        regWriteU16(self, BQ_REG_TS_TEMP, tm.tsRaw);
        regWriteU16(self, BQ_REG_TDIE_TEMP, tm.tdieRaw);
    }
    else
    {
        self->latchedFaults |= BQ_FAULT_I2C_BQ;
    }

    /* Refresh SoC-derived registers (percent stored as 0.1 %). */
    regWriteU16(self, BQ_REG_SOC,
            (uint16_t)(bq25672SocGetPercent(self->soc) * 10u));
    regWriteU16(self, BQ_REG_FULL_MAH,
            (uint16_t)self->soc->cfg.fullCapacityMah);

    /* Charge state: BQ CHG_STAT, or DISCHARGE from the fuel gauge. */
    chgState = driverReadChgState(self->dev);
    if (bq25672SocGetState(self->soc) == BQ25672_STATE_DISCHARGE)
    {
        chgState = BQ_CHG_STATE_DISCHARGE;
    }
    self->regs[BQ_REG_CHG_STATE] = chgState;

    /* Status flags and SoC state. */
    refreshStatus(self, chgState);

    /* Merge newly reported faults into the latched field. */
    self->latchedFaults |= driverReadFaults(self->dev);
    regWriteU16(self, BQ_REG_FAULTS, self->latchedFaults);

    /* Drive the ALERT line while any fault is latched. */
    if (self->event.setAlertLine != NULL)
    {
        self->event.setAlertLine(self->latchedFaults != 0);
    }
}
