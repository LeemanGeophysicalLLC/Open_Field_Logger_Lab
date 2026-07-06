#include "SdFiles.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool isSafeFilename(const char *name) {
  if (name == nullptr) return false;
  const size_t len = strlen(name);
  if (len == 0 || len > 31) return false;
  if (name[0] == '.') return false;  // no leading-dot / hidden files
  if (strstr(name, "..") != nullptr) return false;

  for (size_t i = 0; i < len; i++) {
    const char c = name[i];
    const bool ok = isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

int listRootFiles(SdFileInfo *out, int max_count) {
  File root = SD.open("/");
  if (!root) {
    return 0;
  }

  int count = 0;
  for (File entry = root.openNextFile(); entry && count < max_count; entry = root.openNextFile()) {
    if (!entry.isDirectory()) {
      const char *raw_name = entry.name();
      const char *name = (raw_name[0] == '/') ? raw_name + 1 : raw_name;
      if (isSafeFilename(name)) {
        strlcpy(out[count].name, name, sizeof(out[count].name));
        out[count].size_bytes = static_cast<uint32_t>(entry.size());
        count++;
      }
    }
    entry.close();
  }
  root.close();
  return count;
}

File openForDownload(const char *name) {
  if (!isSafeFilename(name)) {
    return File();
  }
  char path[40];
  snprintf(path, sizeof(path), "/%s", name);
  return SD.open(path, FILE_READ);
}

bool deleteFile(const char *name, bool is_active_log_file) {
  if (!isSafeFilename(name) || is_active_log_file) {
    return false;
  }
  char path[40];
  snprintf(path, sizeof(path), "/%s", name);
  if (!SD.exists(path)) {
    return false;
  }
  return SD.remove(path);
}
