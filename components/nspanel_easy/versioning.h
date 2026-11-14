// versioning.h

#pragma once

#ifdef NSPANEL_EASY_VERSIONING

#include <string>

namespace esphome::nspanel_easy {

/**
 * @brief Compare two CalVer version strings segment by segment.
 *
 * Compares two version strings in the format `YYYY.M.seq` (e.g. "2026.10.2")
 * using numeric comparison per segment. This avoids the lexicographic ordering
 * pitfall where "2026.9.0" would incorrectly sort after "2026.10.0" as strings.
 *
 * The comparison evaluates as: version >= min_version
 *
 * Segment priority (left to right):
 *   1. Year  (YYYY) — most significant
 *   2. Month (M)    — no leading zero, 1–12
 *   3. Seq   (seq)  — release sequence within the month
 *
 * @param version     The version string to test (e.g. the installed blueprint version).
 * @param min_version The minimum required version string to compare against.
 * @return true  if version is equal to or newer than min_version.
 * @return false if version is older than min_version, or if either string is malformed.
 *
 * @note Returns false conservatively when either string cannot be parsed,
 *       to avoid incorrectly passing a version check on bad input.
 * @note Both strings must follow the `YYYY.M.seq` format with no leading
 *       zeros on month or sequence (as produced by the CI/CD versioning workflow).
 *
 * @code
 * calver_gte("2026.10.1", "2026.2.3")  // true  — month 10 > month 2
 * calver_gte("2026.2.3",  "2026.10.1") // false — month 2 < month 10
 * calver_gte("2026.4.1",  "2026.4.1")  // true  — equal
 * calver_gte("",          "2026.4.1")  // false — malformed input
 * @endcode
 */
bool calver_gte(const std::string &version, const std::string &min_version);

/**
 * @brief Check whether a string is a valid TFT version.
 *
 * Two formats are accepted:
 *   - CalVer `YYYY.M.seq` (e.g. "2026.11.17"), carried by the TFT files built
 *     by the CI since the TFT version follows the release version.
 *   - Legacy `major.minor` or bare `major` (e.g. "22.3", "22"), carried by
 *     older TFT files.
 *
 * Each segment holds 1 to 9 digits; empty segments, signs and any other
 * character are rejected.
 *
 * @param version The version string to check (e.g. the TFT version received from the display).
 * @return true  if the string is a valid TFT version in either format.
 * @return false otherwise, including for an empty string.
 *
 * @code
 * tft_ver_valid("2026.11.17")  // true  — CalVer
 * tft_ver_valid("22.3")        // true  — legacy
 * tft_ver_valid("22")          // true  — legacy, bare major
 * tft_ver_valid("22.")         // false — empty segment
 * tft_ver_valid("1.2.3.4")     // false — too many segments
 * @endcode
 */
bool tft_ver_valid(const std::string &version);

/**
 * @brief Compare two TFT version strings.
 *
 * Accepts both formats described in tft_ver_valid(). Segments are compared
 * numerically, which avoids the lexicographic ordering pitfall where "16.2"
 * would incorrectly sort after "16.12" as strings.
 *
 * A legacy version is always older than any CalVer version, as every TFT
 * carrying a CalVer version was built after the legacy versioning ended.
 * A bare legacy integer (e.g. "16") is treated as `major.0`.
 *
 * The comparison evaluates as: version >= min_version
 *
 * @param version     The version string to test (e.g. the TFT version received from the display).
 * @param min_version The minimum required version string to compare against.
 * @return true  if version is equal to or newer than min_version.
 * @return false if version is older than min_version, or if either string is malformed.
 *
 * @note Returns false conservatively when either string cannot be parsed,
 *       to avoid incorrectly passing a version check on bad input.
 *
 * @code
 * tft_ver_gte("16.12",      "16.2")        // true  — minor 12 > minor 2
 * tft_ver_gte("16.2",       "16.12")       // false — minor 2 < minor 12
 * tft_ver_gte("16",         "16.0")        // true  — treated as 16.0 >= 16.0
 * tft_ver_gte("2026.11.17", "22.3")        // true  — CalVer is newer than any legacy version
 * tft_ver_gte("22.5",       "2026.11.17")  // false — legacy is older than any CalVer version
 * tft_ver_gte("2026.10.1",  "2026.9.3")    // true  — month 10 > month 9
 * tft_ver_gte("",           "16.1")        // false — malformed input
 * @endcode
 */
bool tft_ver_gte(const std::string &version, const std::string &min_version);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_VERSIONING
