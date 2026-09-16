#ifndef BQ25672_SOC_H
#define BQ25672_SOC_H

#include "bq25672.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  OCV -> SoC table point (for a SINGLE cell)                         */
/* ------------------------------------------------------------------ */
typedef struct {
    uint16_t ocvMv;     /* cell rest voltage, mV    */
    uint8_t  socPct;    /* corresponding SoC, %     */
} Bq25672OcvPoint;

/* ------------------------------------------------------------------ */
/*  Discharge source: where the current flows / how it is measured.   */
/*  Select according to the hardware topology of the target project.  */
/* ------------------------------------------------------------------ */
typedef enum {
    /* Discharge flows through the BQ (e.g. load on the SYS pin). The  */
    /* BQ measures it directly: IBAT is negative on discharge and      */
    /* positive on charge. The cell is ALWAYS present on the BQ.       */
    BQ25672_DISCHARGE_VIA_BQ = 0,

    /* Discharge is routed away from the BQ by an external relay (the  */
    /* cell is switched to the load). The BQ cannot see it, so a fixed */
    /* current is used while the cell is absent (detected via VBAT).   */
    BQ25672_DISCHARGE_VIA_RELAY,

    /* Discharge current is provided by the host through a callback    */
    /* (external shunt/sensor). Used whenever the cell is on the BQ.   */
    BQ25672_DISCHARGE_EXTERNAL
} Bq25672DischargeSource;

/* ------------------------------------------------------------------ */
/*  Detected system state (for diagnostics/UI)                        */
/* ------------------------------------------------------------------ */
typedef enum {
    BQ25672_STATE_CHARGE = 0,   /* current into the cell (charging)    */
    BQ25672_STATE_IDLE,         /* cell on BQ, at rest (no current)    */
    BQ25672_STATE_DISCHARGE     /* current out of the cell             */
} Bq25672SocState;

/* Optional host callback: returns the present discharge current in mA */
/* (positive magnitude). Used only with BQ25672_DISCHARGE_EXTERNAL.    */
/* Return < 0 to signal a read error (the step is skipped).           */
typedef int32_t (*Bq25672ReadCurrentFn)(void *ctx);

/* ------------------------------------------------------------------ */
/*  Fuel gauge configuration                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    /* OCV table, sorted by ascending ocvMv. */
    const Bq25672OcvPoint *ocvTable;
    uint8_t   ocvTableLen;      /* number of points                    */
    uint8_t   cellCount;        /* cells in series (1..4)              */
    uint32_t  fullCapacityMah;  /* full pack capacity, mAh            */
    int16_t   restCurrentMa;    /* |IBAT| below this => at rest        */
    uint32_t  restTimeMs;       /* rest before OCV recalibration       */
    float     coulombicEff;     /* charge efficiency (0..1), e.g. 0.995 */

    /* --- Discharge handling --- */

    /* How the discharge current is measured. */
    Bq25672DischargeSource dischargeSource;

    /* VIA_RELAY only: constant (approximate) discharge current, mA,   */
    /* applied while the cell is absent from the BQ.                   */
    uint16_t  dischargeCurrentMa;

    /* VIA_RELAY only: per-cell VBAT threshold. Above => cell present  */
    /* on the BQ; below => switched to the load. Set clearly under the */
    /* empty-cell OCV, e.g. ~2000 mV for LiFePO4. Ignored otherwise.   */
    uint16_t  cellPresentMv;

    /* EXTERNAL only: host callback and its context. */
    Bq25672ReadCurrentFn readDischargeCurrent;
    void     *readDischargeCurrentCtx;

    /* --- Charge-state anchoring (CHG_STAT -> SoC) --- */

    /* Enable SoC correction based on the BQ charge state machine. */
    bool      useChargeStateAnchors;

    /* SoC (%) to pull up to on the CC->CV transition (knee).       */
    /* For LiFePO4 typically ~80. Ignored if 0.                     */
    uint8_t   socCcCvKnee;

    /* Upper SoC clamp while in PRECHARGE (0 => disabled). */
    uint8_t   socMaxPrecharge;

    /* Upper SoC clamp while in TRICKLE (0 => disabled). */
    uint8_t   socMaxTrickle;
} Bq25672SocConfig;

/* ------------------------------------------------------------------ */
/*  Fuel gauge state                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    Bq25672Handle    *dev;
    Bq25672SocConfig  cfg;
    Bq25672SocState   state;         /* last detected state           */
    float             socPct;        /* current SoC, 0..100           */
    float             chargeAccMah;  /* accumulated charge, mAh       */
    uint32_t          restAccMs;     /* accumulated rest time         */
    bool              calibrated;
    bool              reconnectPending;  /* OCV calib. after discharge */
    Bq25672ChargeState prevChgState;     /* last CHG_STAT (anchoring) */
} Bq25672Soc;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/* Initialization. Performs an initial OCV calibration from VBAT. */
Bq25672Status bq25672SocInit(Bq25672Soc *fg, Bq25672Handle *dev,
    const Bq25672SocConfig *cfg);

/* Single periodic entry point. Behaviour depends on dischargeSource:  */
/*   - VIA_BQ    : signed IBAT drives charge and discharge; OCV        */
/*                 recalibration at rest.                              */
/*   - VIA_RELAY : cell presence detected from VBAT; BQ IBAT while     */
/*                 present, fixed dischargeCurrentMa while absent.     */
/*   - EXTERNAL  : BQ IBAT for charge, host callback for discharge.    */
/* Call once every deltaMs regardless of the relay/load state.        */
Bq25672Status bq25672SocUpdate(Bq25672Soc *fg, uint32_t deltaMs);

/* Last detected state (for diagnostics/UI). */
Bq25672SocState bq25672SocGetState(const Bq25672Soc *fg);

/* Current percentage (integer, 0..100). */
uint8_t bq25672SocGetPercent(const Bq25672Soc *fg);

/* Forced OCV calibration for a given cell voltage. */
Bq25672Status bq25672SocCalibrateOcv(Bq25672Soc *fg, uint16_t cellOcvMv);

#ifdef __cplusplus
}
#endif

#endif /* BQ25672_SOC_H */
