// text.cpp

#include "text.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace esphome::nspanel_easy {

std::string adjustDecimalSeparator(const std::string &input, char decimalSeparator) {
  if (decimalSeparator == '.') {
    return input;
  }

  // Find the end of the numeric part
  size_t numericEnd = 0;
  for (; numericEnd < input.size(); ++numericEnd) {
    const char c = input[numericEnd];
    if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == ',')) {
      break;
    }
  }

  // If no numeric part found, return original
  if (numericEnd == 0) {
    return input;
  }

  std::string numericPart = input.substr(0, numericEnd);

  // Validate that numericPart is actually a valid number
  char *end;
  strtod(numericPart.c_str(), &end);  // Result unused, only checking validity

  if (end != numericPart.c_str() && *end == '\0') {
    // Find and replace decimal point
    size_t decimalPointPos = numericPart.find('.');
    if (decimalPointPos != std::string::npos) {
      numericPart[decimalPointPos] = decimalSeparator;
    }

    // Append suffix if any
    if (numericEnd < input.size()) {
      numericPart += input.substr(numericEnd);
    }
    return numericPart;
  }

  return input;
}

std::string wrapText(const std::string &text_to_display, uint8_t line_length_limit, uint8_t bytes_per_char) {
  // Safety check for overly long text
  if (text_to_display.length() > 1000) {
    return "ERROR: Text too long";
  }
  // Invalid parameters check
  if (line_length_limit == 0 || bytes_per_char == 0) {
    return "ERROR: Invalid line length";
  }
  // Early exit for already formatted text
  if (text_to_display.find("\\r") != std::string::npos) {
    return text_to_display;
  }

  const uint16_t max_line_length = line_length_limit * bytes_per_char;
  const uint16_t text_len = static_cast<uint16_t>(text_to_display.length());

  // If text is short enough, no wrapping needed
  if (text_len <= max_line_length) {
    return text_to_display;
  }

  // Pre-allocate string to avoid reallocations - optimize for flash memory
  std::string wrapped_text;
  wrapped_text.reserve(text_len + 20);  // Reserve space for line breaks

  uint16_t start = 0;

  while (start < text_len) {
    // Skip leading spaces
    while (start < text_len && text_to_display[start] == ' ') {
      ++start;
    }

    if (start >= text_len) {
      break;  // End of text reached
    }  // if

    // Find end position
    uint16_t end = start + max_line_length;
    if (end >= text_len) {
      end = text_len;
    } else {
      // Find word boundary by looking backwards for space
      uint16_t word_end = end;
      while (word_end > start && text_to_display[word_end] != ' ') {
        --word_end;
      }

      // If we found a space within reasonable distance, use it
      if (word_end > start) {
        end = word_end;
      }
      // Otherwise force break at max_line_length (handles long words)
    }  // else

    // Add text segment
    wrapped_text.append(text_to_display, start, end - start);

    // Add line break if not at end of text
    if (end < text_len) {
      wrapped_text += "\\r";

      // Skip spaces at the break point to avoid leading spaces on next line
      while (end < text_len && text_to_display[end] == ' ') {
        ++end;
      }
    }  // if

    start = end;
  }  // while

  return wrapped_text;
}

bool isStringInList(const std::string &strToSearch, std::initializer_list<std::string> list) {
  return std::any_of(list.begin(), list.end(), [&strToSearch](const std::string &str) { return strToSearch == str; });
}

uint32_t decode_nextion_icon_utf8(const char *bytes, size_t length) {
  // Nextion icons are encoded as 1-3 byte UTF-8 sequences only
  if (!bytes || length == 0 || length > 3) {
    return 0;  // Invalid: null, empty, or too long for Nextion icon
  }

  unsigned char byte = static_cast<unsigned char>(bytes[0]);
  uint32_t code_point = 0;

  // 1-byte sequence (0x00-0x7F)
  if ((byte & 0x80) == 0x00) {
    code_point = byte;
  }
  // 2-byte sequence (0xC0-0xDF)
  else if ((byte & 0xE0) == 0xC0) {
    if (length < 2 || (bytes[1] & 0xC0) != 0x80) {
      return 0;  // Invalid: insufficient length or bad continuation byte
    }
    code_point = ((byte & 0x1F) << 6) | (bytes[1] & 0x3F);
    // Reject overlong encodings
    if (code_point < 0x80) {
      return 0;
    }
  }
  // 3-byte sequence (0xE0-0xEF)
  else if ((byte & 0xF0) == 0xE0) {
    if (length < 3 || (bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) {
      return 0;  // Invalid: insufficient length or bad continuation bytes
    }
    code_point = ((byte & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
    // Reject overlong encodings and surrogate code points
    if (code_point < 0x800 || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
      return 0;
    }
  }
  // Invalid: 4-byte sequences or invalid leading byte
  else {
    return 0;
  }

  return code_point;
}

void replace_all(std::string &str, const char *token, const char *value) {
  const size_t token_len = strlen(token);
  if (token_len == 0)
    return;  // Nothing to replace
  const size_t value_len = strlen(value);
  size_t pos = 0;
  while ((pos = str.find(token, pos)) != std::string::npos) {
    str.replace(pos, token_len, value);
    pos += value_len;  // Advance past the replacement to avoid infinite loop
  }
}

namespace {

/// Inclusive range of Unicode code points.
struct CodePointRange {
  uint32_t first;
  uint32_t last;
};

/// Code points rendered as wide glyphs (about 1.5 times a digit) by the fonts in the TFT.
/// Based on the East Asian Wide and Fullwidth ranges, limited to the Basic Multilingual Plane (the only one
/// supported by Nextion fonts), plus the conjoining Hangul Jamo, which Nextion renders standalone.
constexpr CodePointRange WIDE_CODE_POINTS[] = {
    {0x1100, 0x11FF},  // Hangul Jamo
    {0x2E80, 0x303E},  // CJK Radicals, Kangxi Radicals, Ideographic Description, CJK Symbols and Punctuation
    {0x3041, 0x33FF},  // Hiragana, Katakana, Bopomofo, Hangul Compatibility Jamo, ..., CJK Compatibility
    {0x3400, 0x4DBF},  // CJK Unified Ideographs Extension A
    {0x4E00, 0x9FFF},  // CJK Unified Ideographs
    {0xA000, 0xA4CF},  // Yi Syllables and Radicals
    {0xA960, 0xA97F},  // Hangul Jamo Extended-A
    {0xAC00, 0xD7A3},  // Hangul Syllables
    {0xF900, 0xFAFF},  // CJK Compatibility Ideographs
    {0xFE10, 0xFE19},  // Vertical Forms
    {0xFE30, 0xFE6F},  // CJK Compatibility Forms and Small Form Variants
    {0xFF00, 0xFF60},  // Fullwidth Forms
    {0xFFE0, 0xFFE6},  // Fullwidth Signs
};

/// Weights used by adjusted_text_length(), in half units.
constexpr uint8_t WEIGHT_HALF_WIDTH = 1;  // Narrow characters listed by the caller
constexpr uint8_t WEIGHT_NORMAL = 2;      // Latin digits and letters, and any other character
constexpr uint8_t WEIGHT_WIDE = 3;        // CJK, Hangul, Kana and full-width forms

bool is_wide_code_point(uint32_t code_point) {
  return std::any_of(
      std::begin(WIDE_CODE_POINTS), std::end(WIDE_CODE_POINTS),
      [code_point](const CodePointRange &range) { return code_point >= range.first && code_point <= range.last; });
}

/**
 * @brief Gets the length of the UTF-8 sequence starting at @p pos.
 *
 * @return Length in bytes (1 to 4), or 0 when the lead byte is invalid or the sequence is truncated or has an
 *         invalid continuation byte.
 */
size_t utf8_sequence_length(const std::string &text, size_t pos) {
  const auto lead = static_cast<unsigned char>(text[pos]);
  size_t length = 0;
  if ((lead & 0x80) == 0x00) {
    return 1;  // ASCII
  } else if ((lead & 0xE0) == 0xC0) {
    length = 2;
  } else if ((lead & 0xF0) == 0xE0) {
    length = 3;
  } else if ((lead & 0xF8) == 0xF0) {
    length = 4;
  } else {
    return 0;  // Continuation byte or invalid lead byte
  }  // lead
  if (pos + length > text.size()) {
    return 0;  // Truncated sequence
  }
  for (size_t i = 1; i < length; ++i) {
    if ((static_cast<unsigned char>(text[pos + i]) & 0xC0) != 0x80) {
      return 0;  // Invalid continuation byte
    }
  }
  return length;
}

}  // namespace

uint8_t adjusted_text_length(const std::string &text, const std::string &half_width_chars) {
  uint32_t half_units = 0;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t sequence_length = utf8_sequence_length(text, pos);
    if (sequence_length == 0) {
      // Invalid byte: count it as a normal character and resynchronize on the next byte
      half_units += WEIGHT_NORMAL;
      ++pos;
      continue;
    }  // sequence_length == 0
    if (half_width_chars.find(text.data() + pos, 0, sequence_length) != std::string::npos) {
      half_units += WEIGHT_HALF_WIDTH;  // Listed narrow characters take precedence over any other rule
    } else if (sequence_length >= 2 && sequence_length <= 3 &&
               is_wide_code_point(decode_nextion_icon_utf8(text.data() + pos, sequence_length))) {
      half_units += WEIGHT_WIDE;
    } else {
      half_units += WEIGHT_NORMAL;
    }  // half_width_chars
    pos += sequence_length;
  }  // while pos

  const uint32_t length = (half_units + 1) / 2;  // Round up to whole characters
  return static_cast<uint8_t>(std::min<uint32_t>(length, 255));
}

}  // namespace esphome::nspanel_easy
