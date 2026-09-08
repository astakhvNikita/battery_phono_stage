/* ------------------------------------------------------------------ */
/* bq25672Regmap.h                                                    */
/* I2C-slave register map for the BQ25672 charge module.              */
/* Shared contract between the charger MCU (slave) and the master.    */
/*                                                                    */
/* Access model: master writes [reg, data...] and reads [reg]+STOP.   */
/* Multi-byte fields are little-endian with register auto-increment.  */
/* Units: voltage = mV, current = mA (signed for IBAT), SoC = 0.1 %.  */
/* TS/TDIE are exposed as raw BQ25672 ADC words.                      */
/* ------------------------------------------------------------------ */

#ifndef BQ25672_REGMAP_H
#define BQ25672_REGMAP_H

#include <stdint.h>

/* --- Protocol constants --------------------------------------------- */

#define BQ_SLAVE_I2C_ADDR       0x2A    /* 7-bit slave address        */
#define BQ_WHO_AM_I_VALUE       0xB6    /* Signature at WHO_AM_I       */
#define BQ_PROTO_VERSION        0x01    /* Register-map version        */

/* --- Register addresses --------------------------------------------- */

/* Block 1: identification / system (read-only). */
#define BQ_REG_WHO_AM_I         0x00    /* u8  signature              */
#define BQ_REG_FW_VERSION       0x01    /* u16 major.minor            */
#define BQ_REG_PROTO_VERSION    0x02    /* u8  register-map version   */
#define BQ_REG_UPTIME_S         0x03    /* u32 seconds since boot     */
#define BQ_REG_HEARTBEAT        0x04    /* u8  rolling counter        */

/* Block 2: measurements / telemetry (read-only). */
#define BQ_REG_VBAT_MV          0x10    /* u16 battery voltage        */
#define BQ_REG_VSYS_MV          0x12    /* u16 system voltage         */
#define BQ_REG_VBUS_MV          0x14    /* u16 input voltage          */
#define BQ_REG_IBAT_MA          0x16    /* s16 battery current        */
#define BQ_REG_IBUS_MA          0x18    /* u16 input current          */
#define BQ_REG_TS_TEMP          0x1A    /* u16 raw TS_ADC word        */
#define BQ_REG_TDIE_TEMP        0x1C    /* u16 raw TDIE_ADC word      */
#define BQ_REG_SOC              0x1E    /* u16 state of charge 0.1%   */
#define BQ_REG_SOH              0x20    /* u16 state of health 0.1%   */
#define BQ_REG_REMAIN_MAH       0x22    /* u16 remaining capacity     */
#define BQ_REG_FULL_MAH         0x24    /* u16 full capacity          */
#define BQ_REG_CYCLE_COUNT      0x26    /* u16 charge cycle count     */

/* Block 3: status / state (read-only). */
#define BQ_REG_CHG_STATE        0x30    /* u8  charge state enum      */
#define BQ_REG_VBUS_STATUS      0x31    /* u8  input type/presence    */
#define BQ_REG_SOC_STATE        0x32    /* u8  SoC state enum         */
#define BQ_REG_FLAGS            0x33    /* u16 status bitfield        */
#define BQ_REG_FAULTS           0x35    /* u16 latched fault bitfield */
#define BQ_REG_WARNINGS         0x37    /* u16 warning bitfield       */

/* Block 4: control / setpoints (read-write). */
#define BQ_REG_CHG_ENABLE       0x40    /* u8  0=stop 1=charge        */
#define BQ_REG_CHG_CURRENT_MA   0x41    /* u16 charge current ICHG    */
#define BQ_REG_CHG_VOLTAGE_MV   0x43    /* u16 charge voltage VREG    */
#define BQ_REG_IN_ILIM_MA       0x45    /* u16 input current IINDPM   */
#define BQ_REG_IN_VLIM_MV       0x47    /* u16 input voltage VINDPM   */
#define BQ_REG_ITERM_MA         0x49    /* u16 termination current    */
#define BQ_REG_IPRECHG_MA       0x4B    /* u16 precharge current      */
#define BQ_REG_TS_PROFILE       0x4D    /* u8  JEITA / NTC profile    */
#define BQ_REG_WATCHDOG_S       0x4E    /* u8  BQ watchdog timeout    */
#define BQ_REG_CMD              0x4F    /* u8  command opcode (WO)    */

/* Block 5: SoC configuration / persist (read-write). */
#define BQ_REG_DESIGN_CAP_MAH   0x60    /* u16 full capacity mAh      */
#define BQ_REG_COULOMBIC_EFF    0x62    /* u16 charge efficiency 0.1% */
#define BQ_REG_DISCHARGE_SRC    0x64    /* u8  discharge source enum  */
#define BQ_REG_DISCHARGE_MA     0x65    /* u16 fixed discharge mA     */
#define BQ_REG_EXT_DISCHG_MA    0x67    /* u16 host discharge mA      */
#define BQ_REG_REST_THRESH_MA   0x69    /* u16 rest current threshold */
#define BQ_REG_REST_TIME_S      0x6B    /* u16 rest time for OCV      */
#define BQ_REG_SOC_OVERRIDE     0x6D    /* u16 forced OCV mV          */

#define BQ_REG_MAP_SIZE         0x80    /* Total register-map span    */

/* --- CHG_STATE enum (reg 0x30) -------------------------------------- */
/* Mirrors the BQ25672 CHG_STAT field plus a discharge state.         */

typedef enum
{
    BQ_CHG_STATE_NOT_CHARGING   = 0,
    BQ_CHG_STATE_TRICKLE        = 1,
    BQ_CHG_STATE_PRECHARGE      = 2,
    BQ_CHG_STATE_FAST_CC        = 3,
    BQ_CHG_STATE_TAPER_CV       = 4,
    BQ_CHG_STATE_TOPOFF         = 5,
    BQ_CHG_STATE_DONE           = 6,
    BQ_CHG_STATE_DISCHARGE      = 7
} bq25672RegChgState_t;

/* --- SOC_STATE enum (reg 0x32) -------------------------------------- */
/* Mirrors Bq25672SocState from bq25672Soc.h.                         */

typedef enum
{
    BQ_SOC_STATE_CHARGE         = 0,
    BQ_SOC_STATE_IDLE           = 1,
    BQ_SOC_STATE_DISCHARGE      = 2
} bq25672RegSocState_t;

/* --- FLAGS bitfield (reg 0x33) -------------------------------------- */

#define BQ_FLAG_CHARGING        (1u << 0)
#define BQ_FLAG_DISCHARGING     (1u << 1)
#define BQ_FLAG_RESTING         (1u << 2)
#define BQ_FLAG_CHARGE_DONE     (1u << 3)
#define BQ_FLAG_INPUT_PRESENT   (1u << 4)
#define BQ_FLAG_THERMAL_REG     (1u << 5)
#define BQ_FLAG_SOC_CALIBRATED  (1u << 6)
#define BQ_FLAG_RECONNECT_PEND  (1u << 7)

/* --- FAULTS bitfield (reg 0x35, latched) ---------------------------- */

#define BQ_FAULT_VBAT_OVP       (1u << 0)
#define BQ_FAULT_VBUS_OVP       (1u << 1)
#define BQ_FAULT_IBAT_OCP       (1u << 2)
#define BQ_FAULT_TS_HOT         (1u << 3)
#define BQ_FAULT_TS_COLD        (1u << 4)
#define BQ_FAULT_TDIE_SHUTDOWN  (1u << 5)
#define BQ_FAULT_I2C_BQ         (1u << 6)
#define BQ_FAULT_WATCHDOG_BQ    (1u << 7)
#define BQ_FAULT_SAFETY_TIMER   (1u << 8)

/* --- WARNINGS bitfield (reg 0x37) ----------------------------------- */

#define BQ_WARN_TS_WARM         (1u << 0)
#define BQ_WARN_TS_COOL         (1u << 1)
#define BQ_WARN_VINDPM          (1u << 2)
#define BQ_WARN_IINDPM          (1u << 3)
#define BQ_WARN_LOW_SOC         (1u << 4)

/* --- CMD opcodes (reg 0x4F, write-only) ----------------------------- */

typedef enum
{
    BQ_CMD_NONE                 = 0x00,
    BQ_CMD_CLEAR_FAULTS         = 0x01,
    BQ_CMD_RESET_SOC_FROM_OCV   = 0x02,
    BQ_CMD_RESET_COULOMB        = 0x03,
    BQ_CMD_SAVE_CONFIG          = 0x04,
    BQ_CMD_LOAD_DEFAULTS        = 0x05,
    BQ_CMD_ENTER_SHIP_MODE      = 0x06,
    BQ_CMD_EXIT_SHIP_MODE       = 0x07,
    BQ_CMD_BQ_HARD_RESET        = 0x08,
    BQ_CMD_MCU_REBOOT           = 0x09,
    BQ_CMD_SET_EXT_DISCHG       = 0x0A
} bq25672RegCmd_t;

#endif /* BQ25672_REGMAP_H */
