/**
 * @file settings.h
 * @brief Primary machine configuration and operational logic.
 * @details Defines the core identity and safety behavior of the hardware node.
 * This file serves as the top-level instance for machine parameters.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * @name Machine Identity
 * @details Identity strings used for backend routing and logging.
 * * Available Machine Types (MACHINE_NAME):
 * - tuer, lasercutter, LaserXTool, 3dprinter, metal-mill, lathe, 
 * - embroiderymachine, cncmill-wood, cncmill-metal, 3dprinter-xl, 
 * - panel-saw, wood-planer, wood-bandsaw, miter-saw, wood-routertable
 */
///@{

/** @brief Machine type identifier. Must match backend database entries. */
#define MACHINE_NAME "3dprinter"

/** @brief Unique hardware ID for this specific unit. */
#define MACHINE_ID   "Drucker3"
///@}

/**
 * @name Operational Logic
 * @details Safety and authentication behavior.
 */
///@{

/** * @brief Presence Detection Mode.
 * @details 
 * - true:  Constant authentication. Machine stops if card is removed (Dead-man switch).
 * - false: Single sign-on. Card is only required for the start trigger.
 */
#define RFIDCARD_AUTH_CONST true
///@}

#endif // SETTINGS_H