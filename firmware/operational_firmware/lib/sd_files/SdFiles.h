#pragma once

// SdFiles — read-only browsing plus download/delete for the File Explorer
// tab.
//
// This is intentionally a separate module from SdLogger: SdLogger only
// knows how to write the *current* session's file as fast and safely as
// possible, while this module is about safely exposing the *whole SD card*
// to HTTP requests coming from a student's browser. Mixing those concerns
// into one class would make the "how do I log a row" code harder to read,
// so they're split.
//
// Every function that takes a filename from the web layer must go through
// isSafeFilename() first — this is the single choke point that stops a
// student (accidentally or otherwise) from requesting something like
// "../config" or an absolute path and reaching outside the SD card's root
// directory.

#include <SD.h>
#include <stdint.h>

#include "logger_types.h"

// Returns true only for names made up of letters, digits, '.', '_', '-'
// (1-31 characters), with no ".." anywhere and no leading '.'. This is
// deliberately strict — it rejects anything that isn't a plain flat
// filename, since the SD root here never has subdirectories.
bool isSafeFilename(const char *name);

// Fills `out` with up to `max_count` files found at SD root (directories
// are skipped). Returns the number of entries written.
int listRootFiles(SdFileInfo *out, int max_count);

// Opens a file for download. Returns a falsy File if the name fails
// isSafeFilename() or the file doesn't exist.
File openForDownload(const char *name);

// Deletes a file. Refuses (returns false) if the name fails
// isSafeFilename(), the file doesn't exist, or `is_active_log_file` is true
// (the caller passes true when `name` matches the session file SdLogger is
// currently writing, so a student can't delete today's in-progress log).
bool deleteFile(const char *name, bool is_active_log_file);
