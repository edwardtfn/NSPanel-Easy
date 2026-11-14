// versioning.cpp

#ifdef NSPANEL_EASY_VERSIONING

#include "versioning.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::nspanel_easy {

bool calver_gte(const std::string &version, const std::string &min_version) {
  int ver_y = 0, ver_m = 0, ver_s = 0;
  int min_y = 0, min_m = 0, min_s = 0;

  // Parse both version strings — fail safe if either is malformed or empty.
  // An empty string produces 0 fields parsed, triggering the != 3 guard below.
  // sscanf with exact field count check (!=3) is safe here; NOLINT
  // suppresses cert-err34-c which flags sscanf for non-numeric input,
  // but malformed input is handled by the field count check below.
  if (sscanf(version.c_str(), "%d.%d.%d", &ver_y, &ver_m, &ver_s) != 3 ||      // NOLINT(cert-err34-c)
      sscanf(min_version.c_str(), "%d.%d.%d", &min_y, &min_m, &min_s) != 3) {  // NOLINT(cert-err34-c)
    return false;
  }

  // Compare year first, then month, then sequence number.
  // Returns false conservatively on any older segment.
  if (ver_y != min_y)
    return ver_y > min_y;
  if (ver_m != min_m)
    return ver_m > min_m;
  return ver_s >= min_s;
}

/// Comparable form of a TFT version: {format, segment 1, segment 2, segment 3}
using TftVersionParts = std::array<uint32_t, 4>;

/// Format marker of TftVersionParts: legacy versions sort before CalVer ones
static constexpr uint32_t TFT_VERSION_FORMAT_LEGACY = 0;
static constexpr uint32_t TFT_VERSION_FORMAT_CALVER = 1;

/// Longest accepted segment, keeping every segment within uint32_t
static constexpr size_t TFT_VERSION_MAX_DIGITS = 9;

/**
 * @brief Parse a TFT version into its comparable form.
 *
 * A legacy `major[.minor]` becomes {LEGACY, major, minor, 0} and a CalVer
 * `YYYY.M.seq` becomes {CALVER, YYYY, M, seq}, so comparing the arrays
 * lexicographically compares the versions.
 *
 * @param version The version string to parse.
 * @param parts   Receives the parsed version; left unspecified on failure.
 * @return true if the version is valid, false otherwise.
 */
static bool parse_tft_version(const std::string &version, TftVersionParts &parts) {
  std::array<uint32_t, 3> segments{};
  size_t count = 0;   // Segments completed so far
  size_t digits = 0;  // Digits in the current segment

  for (const char c : version) {
    if (c == '.') {
      if (digits == 0 || count >= segments.size() - 1)
        return false;  // Empty segment, or more than three segments
      count++;
      digits = 0;
    } else if (c >= '0' && c <= '9') {
      if (++digits > TFT_VERSION_MAX_DIGITS)
        return false;  // Segment too long
      segments[count] = (segments[count] * 10) + static_cast<uint32_t>(c - '0');
    } else {
      return false;  // Not a digit or a dot
    }  // if (c == '.')
  }  // for each character

  if (digits == 0)
    return false;  // Empty string, or empty last segment
  count++;

  if (count == segments.size()) {
    parts = {TFT_VERSION_FORMAT_CALVER, segments[0], segments[1], segments[2]};
  } else {
    parts = {TFT_VERSION_FORMAT_LEGACY, segments[0], segments[1], 0};
  }  // if (count == segments.size())
  return true;
}

bool tft_ver_valid(const std::string &version) {
  TftVersionParts parts{};
  return parse_tft_version(version, parts);
}

bool tft_ver_gte(const std::string &version, const std::string &min_version) {
  TftVersionParts ver{};
  TftVersionParts min{};

  if (!parse_tft_version(version, ver) || !parse_tft_version(min_version, min))
    return false;  // Fail conservatively on empty or unparseable input

  // Lexicographic: format first (legacy < CalVer), then each segment
  return ver >= min;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_VERSIONING
