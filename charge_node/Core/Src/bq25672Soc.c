#include "bq25672Soc.h"

/* ------------------------------------------------------------------ */
/*  Linear interpolation: cell OCV -> SoC%                            */
/* ------------------------------------------------------------------ */
static float bq25672SocFromOcv(const Bq25672Soc *fg, uint16_t cellOcvMv)
{
    const Bq25672OcvPoint *t = fg->cfg.ocvTable;
    uint8_t n = fg->cfg.ocvTableLen;

    if (n == 0) return fg->socPct;

    if (cellOcvMv <= t[0].ocvMv)       return (float)t[0].socPct;
    if (cellOcvMv >= t[n - 1].ocvMv)   return (float)t[n - 1].socPct;

    for (uint8_t i = 1; i < n; ++i) {
        if (cellOcvMv <= t[i].ocvMv) {
            uint16_t v0 = t[i - 1].ocvMv, v1 = t[i].ocvMv;
            float    s0 = t[i - 1].socPct, s1 = t[i].socPct;
            float    k  = (float)(cellOcvMv - v0) / (float)(v1 - v0);
            return s0 + k * (s1 - s0);
        }
    }
    return (float)t[n - 1].socPct;
}

static float bq25672SocClamp(float v)
{
    if (v < 0.0f)   return 0.0f;
    if (v > 100.0f) return 100.0f;
    return v;
}

static void bq25672SocApplyCharge(Bq25672Soc *fg)
{
    if (fg->chargeAccMah < 0.0f)
        fg->chargeAccMah = 0.0f;
    if (fg->chargeAccMah > (float)fg->cfg.fullCapacityMah)
        fg->chargeAccMah = (float)fg->cfg.fullCapacityMah;
    fg->socPct = bq25672SocClamp(
        fg->chargeAccMah / (float)fg->cfg.fullCapacityMah * 100.0f);
}

/* Integrate a signed current (mA, +charge / -discharge) over deltaMs. */
static void bq25672SocIntegrate(Bq25672Soc *fg, float currentMa,
    uint32_t deltaMs)
{
    float dMah = currentMa * (float)deltaMs / 3600000.0f;
    /* Charge efficiency applies to charge only. */
    if (dMah > 0.0f)
        dMah *= fg->cfg.coulombicEff;
    fg->chargeAccMah += dMah;
    bq25672SocApplyCharge(fg);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */
Bq25672Status bq25672SocCalibrateOcv(Bq25672Soc *fg, uint16_t cellOcvMv)
{
    if (!fg) return BQ25672_ERR_PARAM;
    fg->socPct       = bq25672SocClamp(bq25672SocFromOcv(fg, cellOcvMv));
    fg->chargeAccMah = fg->socPct / 100.0f * (float)fg->cfg.fullCapacityMah;
    fg->calibrated   = true;
    return BQ25672_OK;
}

Bq25672Status bq25672SocInit(Bq25672Soc *fg, Bq25672Handle *dev,
    const Bq25672SocConfig *cfg)
{
    uint16_t vbatMv;
    Bq25672Status st;

    if (!fg || !dev || !cfg || !cfg->ocvTable || cfg->ocvTableLen == 0 ||
            cfg->cellCount == 0 || cfg->fullCapacityMah == 0)
        return BQ25672_ERR_PARAM;

    fg->dev = dev;
    fg->cfg = *cfg;
    if (fg->cfg.coulombicEff <= 0.0f || fg->cfg.coulombicEff > 1.0f)
        fg->cfg.coulombicEff = 1.0f;
    if (fg->cfg.cellPresentMv == 0)
        fg->cfg.cellPresentMv = 2000;   /* safe default for LiFePO4 */

    fg->state            = BQ25672_STATE_IDLE;
    fg->socPct           = 0.0f;
    fg->chargeAccMah     = 0.0f;
    fg->restAccMs        = 0;
    fg->calibrated       = false;
    fg->reconnectPending = false;

    st = bq25672ReadVbatMv(dev, &vbatMv);
    if (st != BQ25672_OK) return st;

    return bq25672SocCalibrateOcv(fg,
        (uint16_t)(vbatMv / fg->cfg.cellCount));
}

/* ------------------------------------------------------------------ */
/*  Rest detection + OCV recalibration. Pass |current| in mA.         */
/* ------------------------------------------------------------------ */
static void bq25672SocHandleRest(Bq25672Soc *fg, int16_t absCurrentMa,
    uint16_t vbatMv, uint32_t deltaMs)
{
    if (absCurrentMa < fg->cfg.restCurrentMa) {
        fg->state = BQ25672_STATE_IDLE;
        fg->restAccMs += deltaMs;
        if (fg->reconnectPending || fg->restAccMs >= fg->cfg.restTimeMs) {
            uint16_t cellMv = (uint16_t)(vbatMv / fg->cfg.cellCount);
            bq25672SocCalibrateOcv(fg, cellMv);
            fg->restAccMs = 0;
            fg->reconnectPending = false;
        }
    } else {
        fg->restAccMs = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  VIA_BQ: discharge current measured directly by the BQ.            */
/* ------------------------------------------------------------------ */
static Bq25672Status bq25672SocUpdateViaBq(Bq25672Soc *fg, uint32_t deltaMs,
    uint16_t vbatMv)
{
    int16_t ibatMa, absIbat;
    Bq25672Status st = bq25672ReadIbatMa(fg->dev, &ibatMa);
    if (st != BQ25672_OK) return st;

    absIbat = (ibatMa >= 0) ? ibatMa : (int16_t)(-ibatMa);

    if (absIbat >= fg->cfg.restCurrentMa) {
        fg->state = (ibatMa > 0)
            ? BQ25672_STATE_CHARGE
            : BQ25672_STATE_DISCHARGE;
        bq25672SocIntegrate(fg, (float)ibatMa, deltaMs);
    }
    bq25672SocHandleRest(fg, absIbat, vbatMv, deltaMs);
    return BQ25672_OK;
}

/* ------------------------------------------------------------------ */
/*  VIA_RELAY: auto-detect cell presence from VBAT.                   */
/* ------------------------------------------------------------------ */
static Bq25672Status bq25672SocUpdateViaRelay(Bq25672Soc *fg,
    uint32_t deltaMs, uint16_t vbatMv)
{
    uint16_t cellMv = (uint16_t)(vbatMv / fg->cfg.cellCount);

    if (cellMv >= fg->cfg.cellPresentMv) {
        /* Cell on BQ: count charge from IBAT (>0), OCV calib. at rest. */
        int16_t ibatMa, absIbat;
        Bq25672Status st = bq25672ReadIbatMa(fg->dev, &ibatMa);
        if (st != BQ25672_OK) return st;

        absIbat = (ibatMa >= 0) ? ibatMa : (int16_t)(-ibatMa);
        if (ibatMa > 0) {
            fg->state = BQ25672_STATE_CHARGE;
            bq25672SocIntegrate(fg, (float)ibatMa, deltaMs);
        }
        bq25672SocHandleRest(fg, absIbat, vbatMv, deltaMs);
    } else {
        /* Cell switched to the load: fixed discharge current. */
        if (fg->state != BQ25672_STATE_DISCHARGE) {
            fg->reconnectPending = true;   /* recalibrate on return */
            fg->restAccMs = 0;
        }
        fg->state = BQ25672_STATE_DISCHARGE;
        if (fg->cfg.dischargeCurrentMa > 0)
            bq25672SocIntegrate(fg,
                -(float)fg->cfg.dischargeCurrentMa, deltaMs);
    }
    return BQ25672_OK;
}

/* ------------------------------------------------------------------ */
/*  EXTERNAL: discharge current supplied by a host callback.          */
/* ------------------------------------------------------------------ */
static Bq25672Status bq25672SocUpdateExternal(Bq25672Soc *fg,
    uint32_t deltaMs, uint16_t vbatMv)
{
    int16_t ibatMa = 0, absIbat;
    int32_t dis;
    Bq25672Status st = bq25672ReadIbatMa(fg->dev, &ibatMa);
    if (st != BQ25672_OK) return st;

    if (ibatMa > 0) {
        /* Charging: prefer the BQ measurement. */
        absIbat = ibatMa;
        fg->state = BQ25672_STATE_CHARGE;
        bq25672SocIntegrate(fg, (float)ibatMa, deltaMs);
    } else {
        /* Not charging: ask the host for the discharge current. */
        dis = fg->cfg.readDischargeCurrent
            ? fg->cfg.readDischargeCurrent(fg->cfg.readDischargeCurrentCtx)
            : -1;
        if (dis < 0) dis = 0;
        absIbat = (int16_t)dis;
        if (dis >= (int32_t)fg->cfg.restCurrentMa) {
            fg->state = BQ25672_STATE_DISCHARGE;
            bq25672SocIntegrate(fg, -(float)dis, deltaMs);
        }
    }
    bq25672SocHandleRest(fg, absIbat, vbatMv, deltaMs);
    return BQ25672_OK;
}

Bq25672Status bq25672SocUpdate(Bq25672Soc *fg, uint32_t deltaMs)
{
    uint16_t vbatMv;
    Bq25672Status st;

    if (!fg || !fg->dev) return BQ25672_ERR_PARAM;

    st = bq25672ReadVbatMv(fg->dev, &vbatMv);
    if (st != BQ25672_OK) return st;

    switch (fg->cfg.dischargeSource) {
        case BQ25672_DISCHARGE_VIA_RELAY:
            return bq25672SocUpdateViaRelay(fg, deltaMs, vbatMv);
        case BQ25672_DISCHARGE_EXTERNAL:
            return bq25672SocUpdateExternal(fg, deltaMs, vbatMv);
        case BQ25672_DISCHARGE_VIA_BQ:
        default:
            return bq25672SocUpdateViaBq(fg, deltaMs, vbatMv);
    }
}

Bq25672SocState bq25672SocGetState(const Bq25672Soc *fg)
{
    if (!fg) return BQ25672_STATE_IDLE;
    return fg->state;
}

uint8_t bq25672SocGetPercent(const Bq25672Soc *fg)
{
    if (!fg) return 0;
    return (uint8_t)(fg->socPct + 0.5f);
}
