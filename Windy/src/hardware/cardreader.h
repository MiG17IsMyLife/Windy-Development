#pragma once

#include <stdint.h>
#include <stdbool.h>

// Card insert trigger - set by input system when CardInsert is pressed
extern bool g_triggerInsertKey;

/**
 * @brief Search the ELF file's .symtab for a mangled C++ function name.
 * This is the Windy equivalent of lindbergh-loader's findStaticFnAddr().
 * @param functionName Mangled C++ name (e.g. "_ZN11cFileCardIF9emulationEv")
 * @return Address of the function, or nullptr if not found
 */
void* FindStaticFnAddr(const char* functionName);

/**
 * @brief Apply card reader patches for the current game (call from Patches::Apply)
 */
void ApplyCardReaderPatches();

/**
 * @brief Check if a card file exists with the expected size
 * @param folderPath Folder to search (empty = current directory)
 * @param expectedSize Expected file size in bytes
 * @param twoDigits Use 2-digit numbering (ID4 JAP) vs 3-digit
 * @return true if a matching card file exists
 */
bool IdCardFileExists(const char* folderPath, long expectedSize, bool twoDigits);

/**
 * @brief Patch memory for card eject behavior (called after card file header is written)
 */
void IdPatchEject();
