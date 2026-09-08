/* ------------------------------------------------------------------ */
/* bq25672Slave.h                                                     */
/* I2C-slave register-map handler for the BQ25672 charge module.      */
/* Bridges the master-facing register map to the bq25672 driver and   */
/* the bq25672Soc fuel gauge. Platform independent: the caller feeds  */
/* raw I2C-slave events and provides optional persist/alert HAL.      */
/* ------------------------------------------------------------------ */

#ifndef BQ25672_SLAVE_H
#define BQ25672_SLAVE_H

#include <stdint.h>
#include <stdbool.h>
#include "bq25672.h"
#include "bq25672Soc.h"
#include "bq25672Regmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Persistence HAL ------------------------------------------------ */
/* Optional non-volatile storage for the config block (SAVE/LOAD).    */

typedef struct
{
    bool (*save)(const uint8_t *data, uint16_t len);
    bool (*load)(uint8_t *data, uint16_t len);
} Bq25672SlaveNvmHal;

/* --- Event / notification HAL --------------------------------------- */
/* Drives the ALERT/INT line and optional local reboot.               */

typedef struct
{
    /* Assert (true) or release (false) the ALERT line to master. */
    void (*setAlertLine)(bool asserted);

    /* Reboot the local MCU (BQ_CMD_MCU_REBOOT). May be NULL. */
    void (*mcuReboot)(void);
} Bq25672SlaveEventHal;

/* --- Slave handler context ------------------------------------------ */

typedef struct
{
    Bq25672Handle           *dev;   /* charger driver instance    */
    Bq25672Soc              *soc;   /* fuel gauge instance        */
    Bq25672SlaveNvmHal      nvm;    /* persistence callbacks      */
    Bq25672SlaveEventHal    event;  /* alert / reboot callbacks   */

    /* Shadow register file exposed on the I2C bus. */
    uint8_t                 regs[BQ_REG_MAP_SIZE];

    /* Current transaction cursor (register auto-increment). */
    uint8_t                 cursor;
    bool                    cursorSet;

    /* Latched faults, cleared only by BQ_CMD_CLEAR_FAULTS. */
    uint16_t                latchedFaults;

    uint8_t                 heartbeat;
    uint32_t                uptimeS;
} Bq25672Slave;

/* --- API ------------------------------------------------------------ */

/* Initialize the handler and bind driver / SoC / HAL instances. */
bool bq25672SlaveInit(Bq25672Slave *self,
        Bq25672Handle *dev,
        Bq25672Soc *soc,
        const Bq25672SlaveNvmHal *nvm,
        const Bq25672SlaveEventHal *event);

/* I2C-slave callback: master issued a start / repeated start. */
void bq25672SlaveOnStart(Bq25672Slave *self);

/* I2C-slave callback: one byte received from master. The first byte
 * of a write frame selects the register; the following bytes are
 * payload written at the auto-incrementing cursor. Returns true if
 * the byte was accepted (ACK). */
bool bq25672SlaveOnWrite(Bq25672Slave *self, uint8_t byte);

/* I2C-slave callback: master requests one byte for a read. Returns
 * the byte at the current cursor and advances it. */
uint8_t bq25672SlaveOnRead(Bq25672Slave *self);

/* Periodic service: refreshes telemetry into the register file,
 * evaluates faults, drives the ALERT line and updates uptime.
 * Call at a fixed cadence (for example every 100 ms). */
void bq25672SlaveTask(Bq25672Slave *self, uint32_t elapsedMs);

#ifdef __cplusplus
}
#endif

#endif /* BQ25672_SLAVE_H */
