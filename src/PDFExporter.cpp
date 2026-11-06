#include "PDFExporter.h"
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Line-type classification
// ---------------------------------------------------------------------------

enum class LineType {
  BLANK,          // empty or only whitespace
  SECTION_HEADER, // "Experience", "Projects", "Education", …
  SUB_HEADER,     // line containing " | " – company/role separator
  BULLET,         // starts with -, *, or •
  NAME_LINE,      // first non-blank line of the document (candidate name)
  CONTACT_LINE,   // second non-blank line (phone | email | linkedin)
  BODY            // everything else
};

struct ClassifiedLine {
  LineType type;
  std::string text; // cleaned, PDF-escaped content
  std::string raw;  // original text before escaping
};

// Well-known resume section header words (lowercase for comparison)
static const std::set<std::string> SECTION_KEYWORDS = {
    "education",
    "experience",
    "projects",
    "skills",
    "technical skills",
    "languages",
    "certifications",
    "awards",
    "publications",
    "volunteer",
    "interests",
    "summary",
    "objective",
    "research",
    "leadership",
    "activities",
    "extracurricular",
    "courses",
    "coursework",
    "work experience",
    "professional experience",
    "academic experience"};

/**
 * @brief Removes leading and trailing whitespace from a string.
 *
 * Detailed description: Finds the first and last non-whitespace characters
 * and extracts the substring between them. If the string is entirely
 * whitespace, it returns an empty string.
 *
 * @author David
 * @param s The string to trim.
 * @return std::string The trimmed string.
 */
static std::string trimWhitespace(const std::string &s) {
  size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

/**
 * @brief Converts a string to lowercase.
 *
 * Detailed description: Iterates through each character in the string
 * and applies std::tolower. Useful for case-insensitive comparisons
 * such as identifying section headers.
 *
 * @author David
 * @param s The string to convert.
 * @return std::string The lowercased string.
 */
static std::string toLower(const std::string &s) {
  std::string r = s;
  for (char &c : r)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return r;
}

/**
 * @brief Escapes special characters for PDF string streams.
 *
 * Detailed description: In PDF text streams, parentheses and backslashes
 * are reserved and must be escaped with a backslash. It also drops
 * non-printable or non-ASCII characters to prevent PDF corruption.
 *
 * @author David
 * @param s The raw string to escape.
 * @return std::string The escaped string safe for PDF Tj operators.
 */
static std::string pdfEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (unsigned char c : s) {
    if (c == '(')
      out += "\\(";
    else if (c == ')')
      out += "\\)";
    else if (c == '\\')
      out += "\\\\";
    else if (c < 0x20 || c > 0x7E)
      out += ' '; // drop non-printable/non-ASCII
    else
      out += static_cast<char>(c);
  }
  return out;
}

/**
 * @brief Determines if a line is likely a resume section header.
 *
 * Detailed description: Checks if the line exactly matches a predefined set
 * of common resume headers (e.g., "experience", "education"). It also
 * uses heuristics to identify short, all-caps strings that often serve
 * as headers.
 *
 * @author David
 * @param trimmed The trimmed text line.
 * @return true If the line is classified as a section header.
 * @return false Otherwise.
 */
static bool isSectionHeader(const std::string &trimmed) {
  if (trimmed.empty())
    return false;
  std::string lower = toLower(trimmed);
  // Direct match
  if (SECTION_KEYWORDS.count(lower))
    return true;
  // Also match short all-caps lines that look like headers (2–30 chars, all
  // alpha/space)
  if (trimmed.size() >= 2 && trimmed.size() <= 30) {
    bool allCapsOrSpace = true;
    int alphaCount = 0;
    for (char c : trimmed) {
      if (std::isalpha(static_cast<unsigned char>(c))) {
        alphaCount++;
        if (std::islower(static_cast<unsigned char>(c))) {
          allCapsOrSpace = false;
          break;
        }
      } else if (c != ' ' && c != '\t') {
        allCapsOrSpace = false;
        break;
      }
    }
    if (allCapsOrSpace && alphaCount >= 2)
      return true;
  }
  return false;
}

/**
 * @brief Determines if a line is a bullet point.
 *
 * Detailed description: Checks the first character of the string to see
 * if it matches typical bullet markers like '-', '*', or the UTF-8 bullet '•'.
 *
 * @author Pal
 * @param trimmed The trimmed text line.
 * @return true If the line starts with a bullet point character.
 * @return false Otherwise.
 */
static bool isBulletLine(const std::string &trimmed) {
  if (trimmed.empty())
    return false;
  if (trimmed[0] == '-' || trimmed[0] == '*')
    return true;
  // UTF-8 bullet •  = 0xE2 0x80 0xA2
  if (trimmed.size() >= 3 && (unsigned char)trimmed[0] == 0xE2 &&
      (unsigned char)trimmed[1] == 0x80 && (unsigned char)trimmed[2] == 0xA2)
    return true;
  return false;
}

/**
 * @brief Determines if a line is a sub-header (e.g., job title and dates).
 *
 * Detailed description: Checks for the presence of separators like " | " or " -
 * " typically used by candidates to divide company, role, location, and dates
 * on a single line.
 *
 * @author Pal
 * @param trimmed The trimmed text line.
 * @return true If the line contains sub-header separators.
 * @return false Otherwise.
 */
static bool isSubHeader(const std::string &trimmed) {
  // Lines with " | " are typically "Company | City" or "Role | Dates"
  return trimmed.find(" | ") != std::string::npos ||
         trimmed.find(" - ") != std::string::npos;
}

// ---------------------------------------------------------------------------
// Word-wrapping
// ---------------------------------------------------------------------------

struct WrappedLine {
  LineType type;
  std::string text;  // PDF-escaped, ready to drop into a Tj/Tj stream
  bool firstOfGroup; // first wrapped segment of a logical line
};

/**
 * @brief Wraps a single classified line into one or more PDF display lines.
 *
 * Detailed description: Breaks down long strings of text so they fit within
 * the horizontal width of the PDF page. Bullets are given a shorter column
 * width to account for indentation, and their leading bullet characters are
 * stripped so a standard PDF glyph can be drawn instead.
 *
 * @author Pal
 * @param cl The classified line of text to wrap.
 * @param maxNormal The maximum character width for a normal body line.
 * @param maxBullet The maximum character width for an indented bullet line.
 * @return std::vector<WrappedLine> A list of wrapped lines ready for layout.
 */
static std::vector<WrappedLine> wrapLine(const ClassifiedLine &cl,
                                         size_t maxNormal, size_t maxBullet) {
  std::vector<WrappedLine> out;

  // For bullets, strip the leading symbol
  std::string content = cl.raw;
  if (cl.type == LineType::BULLET) {
    size_t start = cl.raw.find_first_not_of(" \t");
    if (start != std::string::npos) {
      size_t afterBullet = start + 1;
      // UTF-8 bullet occupies 3 bytes
      if ((unsigned char)cl.raw[start] == 0xE2)
        afterBullet = start + 3;
      afterBullet = cl.raw.find_first_not_of(" \t", afterBullet);
      content =
          (afterBullet != std::string::npos) ? cl.raw.substr(afterBullet) : "";
    }
  }

  size_t maxLen = (cl.type == LineType::BULLET) ? maxBullet : maxNormal;

  // Split into words and re-wrap
  std::istringstream wss(content);
  std::string word, current;
  bool first = true;
  while (wss >> word) {
    std::string ew = pdfEscape(word);
    if (current.empty()) {
      current = ew;
    } else if (current.size() + 1 + ew.size() <= maxLen) {
      current += ' ';
      current += ew;
    } else {
      WrappedLine wl;
      wl.type = cl.type;
      wl.text = current;
      wl.firstOfGroup = first;
      out.push_back(wl);
      current = ew;
      first = false;
    }
  }
  if (!current.empty()) {
    WrappedLine wl;
    wl.type = cl.type;
    wl.text = current;
    wl.firstOfGroup = first;
    out.push_back(wl);
  }
  // If the line was empty (blank), still emit one entry so spacing is preserved
  if (out.empty()) {
    WrappedLine wl;
    wl.type = cl.type;
    wl.text = "";
    wl.firstOfGroup = true;
    out.push_back(wl);
  }
  return out;
}

// ---------------------------------------------------------------------------
// PDF page building
// ---------------------------------------------------------------------------

struct PDFPage {
  int pageObjNum;
  int streamObjNum;
  std::string streamData;
};

/**
 * @brief Builds the raw PDF content stream string for a single page.
 *
 * Detailed description: Takes a subset of wrapped lines and positions them
 * using absolute PDF text operators (Tm, Tj). It applies specific fonts,
 * sizes, and vertical leading gaps based on the LineType. It also draws
 * horizontal rules under contact sections and section headers.
 *
 * Font assignments:
 *   F1: Helvetica 11pt (body/bullets)
 *   F2: Helvetica-Bold 12pt (headers)
 *   F3: Helvetica 9pt (contact)
 *   F4: Helvetica-Bold 16pt (name)
 *
 * @author Pal
 * @param lines The full list of wrapped lines for the document.
 * @param startIdx The index of the first line to include on this page.
 * @param endIdx The index of the first line to exclude (end of page).
 * @param fontF1 The object ID for the F1 font.
 * @param fontF2 The object ID for the F2 font.
 * @param isFirstPage True if this is the first page of the document.
 * @return std::string The uncompressed PDF stream data for the page.
 */
static std::string buildPageStream(const std::vector<WrappedLine> &lines,
                                   size_t startIdx, size_t endIdx,
                                   const int fontF1, const int fontF2,
                                   bool isFirstPage) {
  const float LEFT_MARGIN = 54.0f;   // ~0.75 in
  const float BULLET_INDENT = 68.0f; // body + small indent
  const float START_Y = 740.0f;
  const float LEADING_BODY = 12.5f;
  const float LEADING_BULLET = 12.5f;
  const float LEADING_SECTION = 16.0f;   // extra before section header
  const float LEADING_AFTER_SEC = 10.0f; // extra after rule
  const float LEADING_SUBHEADER = 14.0f;
  const float LEADING_BLANK = 14.0f; // gap for blank lines to separate projects
  const float LEADING_NAME = 20.0f;
  const float LEADING_CONTACT = 14.0f;

  std::ostringstream s;
  (void)fontF1;
  (void)fontF2; // used symbolically; fonts are named /F1 /F2 etc.

  float y = START_Y;
  bool inBT = false;

  auto beginText = [&]() {
    if (!inBT) {
      s << "BT\n";
      inBT = true;
    }
  };
  auto endText = [&]() {
    if (inBT) {
      s << "ET\n";
      inBT = false;
    }
  };

  // Helper: move to absolute position
  auto moveTo = [&](float x, float yy) { s << x << " " << yy << " Td\n"; };

  std::string currentFont;
  auto setFont = [&](const std::string &fname, float size) {
    std::string key = fname + std::to_string((int)size);
    if (currentFont != key) {
      s << "/" << fname << " " << size << " Tf\n";
      currentFont = key;
    }
  };

  // Track absolute y so we can do absolute moves
  float curX = LEFT_MARGIN,
        curY = START_Y + LEADING_BODY; // will be set on first move

  auto goTo = [&](float x, float yy) {
    endText();
    beginText();
    // PDF Td is relative, but we reset via a 1 0 0 1 x y Tm matrix
    s << "1 0 0 1 " << x << " " << yy << " Tm\n";
    curX = x;
    curY = yy;
  };

  for (size_t i = startIdx; i < endIdx; ++i) {
    const WrappedLine &wl = lines[i];

    switch (wl.type) {
    case LineType::BLANK: {
      y -= LEADING_BLANK;
      break;
    }
    case LineType::NAME_LINE: {
      goTo(LEFT_MARGIN, y);
      setFont("F4", 16);
      s << "(" << wl.text << ") Tj\n";
      y -= LEADING_NAME;
      break;
    }
    case LineType::CONTACT_LINE: {
      goTo(LEFT_MARGIN, y);
      setFont("F3", 9);
      s << "(" << wl.text << ") Tj\n";
      y -= 4.0f; // Gap between text baseline and rule
      // Draw a thin horizontal rule beneath contact line
      endText();
      s << "0.5 w\n"
        << LEFT_MARGIN << " " << y << " m\n"
        << "558 " << y << " l\n"
        << "S\n";
      y -= 12.0f; // Gap after rule
      break;
    }
    case LineType::SECTION_HEADER: {
      // Extra space before section header
      if (wl.firstOfGroup)
        y -= 10.0f;
      goTo(LEFT_MARGIN, y);
      setFont("F2", 12);
      s << "(" << wl.text << ") Tj\n";
      y -= 4.0f; // Drop rule safely below text descenders
      // Draw underline rule
      endText();
      s << "0.8 w\n"
        << LEFT_MARGIN << " " << y << " m\n"
        << "558 " << y << " l\n"
        << "S\n";
      y -= LEADING_AFTER_SEC; // Guaranteed gap to avoid overlap with following
                              // text ascenders
      break;
    }
    case LineType::SUB_HEADER: {
      if (wl.firstOfGroup)
        y -= 8; // add vertical gap before projects
      goTo(LEFT_MARGIN, y);
      setFont("F2", 11);
      s << "(" << wl.text << ") Tj\n";
      y -= LEADING_SUBHEADER;
      break;
    }
    case LineType::BULLET: {
      // Draw the bullet glyph, then the text with indent
      goTo(LEFT_MARGIN, y);
      setFont("F1", 11);
      s << "(-) Tj\n"; // simple ASCII dash bullet
      goTo(BULLET_INDENT, y);
      setFont("F1", 11);
      s << "(" << wl.text << ") Tj\n";
      y -= LEADING_BULLET;
      break;
    }
    case LineType::BODY:
    default: {
      goTo(LEFT_MARGIN, y);
      setFont("F1", 11);
      s << "(" << wl.text << ") Tj\n";
      y -= LEADING_BODY;
      break;
    }
    }
  }
  endText();
  return s.str();
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * @brief Main execution function to export text into a generated PDF.
 *
 * Detailed description: Implements a 5-step process:
 * 1. Classifies each line of text based on heuristics (headers, bullets).
 * 2. Word-wraps the lines to fit page bounds.
 * 3. Paginates the wrapped lines based on estimated point heights.
 * 4. Assigns valid PDF object IDs for the stream and pages.
 * 5. Serializes all objects and the xref table to disk.
 *
 * @author Pal
 * @param outputPath The file path where the generated PDF will be stored.
 * @param rawText The raw resume text to be parsed and formatted.
 * @return true If the PDF was successfully created and written.
 * @return false If file system operations failed to open the output path.
 */
bool PDFExporter::exportToPDF(const std::string &outputPath,
                              const std::string &rawText) {
  std::ofstream out(outputPath, std::ios::binary);
  if (!out)
    return false;

  // ---- Step 1: classify every logical line ----
  std::vector<ClassifiedLine> classified;
  std::istringstream lineStream(rawText);
  std::string line;
  int nonBlankCount = 0;
  int lastAddedIdx = -1;

  auto endsWithPunctuation = [](const std::string &s) {
    if (s.empty())
      return false;
    char c = s.back();
    return (c == '.' || c == '?' || c == '!' || c == ':' || c == ';');
  };

  while (std::getline(lineStream, line)) {
    std::string trimmed = trimWhitespace(line);

    if (trimmed.empty()) {
      ClassifiedLine cl;
      cl.raw = trimmed;
      cl.text = "";
      cl.type = LineType::BLANK;
      classified.push_back(cl);
      lastAddedIdx = -1;
    } else {
      nonBlankCount++;
      LineType currentType;

      if (nonBlankCount == 1) {
        currentType = LineType::NAME_LINE;
      } else if (nonBlankCount == 2) {
        currentType = LineType::CONTACT_LINE;
      } else if (isSectionHeader(trimmed)) {
        currentType = LineType::SECTION_HEADER;
      } else if (isBulletLine(trimmed)) {
        currentType = LineType::BULLET;
      } else if (isSubHeader(trimmed)) {
        currentType = LineType::SUB_HEADER;
      } else {
        currentType = LineType::BODY;
      }

      bool merged = false;
      // Merge continuations of paragraphs/bullets
      if (lastAddedIdx >= 0 && currentType == LineType::BODY) {
        LineType prevType = classified[lastAddedIdx].type;
        if (prevType == LineType::BULLET || prevType == LineType::BODY) {
          bool wasWrapped = (classified[lastAddedIdx].raw.length() >= 60);
          bool endsWithPunc = endsWithPunctuation(classified[lastAddedIdx].raw);
          bool startsWithLower =
              (!trimmed.empty() &&
               std::islower(static_cast<unsigned char>(trimmed[0])));

          bool mergeIntoBullet = (prevType == LineType::BULLET);

          if (mergeIntoBullet || wasWrapped ||
              (!endsWithPunc && startsWithLower)) {
            classified[lastAddedIdx].raw += " " + trimmed;
            classified[lastAddedIdx].text =
                pdfEscape(classified[lastAddedIdx].raw);
            merged = true;
          }
        }
      }

      if (!merged) {
        ClassifiedLine cl;
        cl.raw = trimmed;
        cl.text = pdfEscape(trimmed);
        cl.type = currentType;
        classified.push_back(cl);
        lastAddedIdx = (int)classified.size() - 1;
      }
    }
  }

  // ---- Step 2: word-wrap all lines ----
  // ~90 chars for body, ~80 for bullets (accounting for indent)
  std::vector<WrappedLine> wrapped;
  for (const auto &cl : classified) {
    auto wls = wrapLine(cl, 90, 80);
    for (auto &w : wls)
      wrapped.push_back(w);
  }

  // ---- Step 3: paginate ----
  // Approximate line heights in points to estimate lines-per-page
  // Page height usable: ~740 - 54 (bottom margin) = 686 pt
  // Average ~14pt leading → ~49 lines per page, but section headers are taller.
  // We use a simple accumulated-height approach.
  const float PAGE_HEIGHT = 686.0f;
  const float NAME_H = 20.0f;
  const float CONTACT_H = 16.0f;
  const float SECTION_H = 24.0f; // header + rule gap
  const float SUBHDR_H = 22.0f;  // 14 + 8 gap
  const float BULLET_H = 12.5f;
  const float BODY_H = 12.5f;
  const float BLANK_H = 14.0f;

  auto lineHeight = [&](const WrappedLine &wl) -> float {
    switch (wl.type) {
    case LineType::NAME_LINE:
      return NAME_H;
    case LineType::CONTACT_LINE:
      return CONTACT_H + 8; // +rule gap
    case LineType::SECTION_HEADER:
      return SECTION_H;
    case LineType::SUB_HEADER:
      return SUBHDR_H;
    case LineType::BULLET:
      return BULLET_H;
    case LineType::BLANK:
      return BLANK_H;
    default:
      return BODY_H;
    }
  };

  // Build page ranges [startIdx, endIdx)
  std::vector<std::pair<size_t, size_t>> pageRanges;
  size_t start = 0;
  float accumulated = 0;
  for (size_t i = 0; i < wrapped.size(); ++i) {
    float h = lineHeight(wrapped[i]);
    if (accumulated + h > PAGE_HEIGHT && i > start) {
      pageRanges.push_back({start, i});
      start = i;
      accumulated = h;
    } else {
      accumulated += h;
    }
  }
  if (start < wrapped.size())
    pageRanges.push_back({start, wrapped.size()});

  // ---- Step 4: assign PDF object IDs ----
  int objId = 1;
  const int catalogId = objId++;
  const int pagesId = objId++;
  const int fontF1Id = objId++; // Helvetica
  const int fontF2Id = objId++; // Helvetica-Bold
  const int fontF3Id = objId++; // Helvetica (contact, smaller)
  const int fontF4Id = objId++; // Helvetica-Bold (name)

  std::vector<PDFPage> pages;
  for (size_t pi = 0; pi < pageRanges.size(); ++pi) {
    PDFPage p;
    p.pageObjNum = objId++;
    p.streamObjNum = objId++;
    p.streamData =
        buildPageStream(wrapped, pageRanges[pi].first, pageRanges[pi].second,
                        fontF1Id, fontF2Id, pi == 0);
    pages.push_back(p);
  }

  // ---- Step 5: serialize ----
  std::vector<long> xref;
  xref.push_back(0); // entry 0 is the free-list head

  out << "%PDF-1.4\n";

  auto writeObjHead = [&](int id) {
    while ((int)xref.size() <= id)
      xref.push_back(0);
    xref[id] = (long)out.tellp();
    out << id << " 0 obj\n";
  };

  writeObjHead(catalogId);
  out << "<< /Type /Catalog /Pages " << pagesId << " 0 R >>\nendobj\n";

  writeObjHead(pagesId);
  out << "<< /Type /Pages /Count " << pages.size() << " /Kids [";
  for (const auto &p : pages)
    out << p.pageObjNum << " 0 R ";
  out << "] >>\nendobj\n";

  writeObjHead(fontF1Id);
  out << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";

  writeObjHead(fontF2Id);
  out << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold "
         ">>\nendobj\n";

  writeObjHead(fontF3Id);
  out << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";

  writeObjHead(fontF4Id);
  out << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold "
         ">>\nendobj\n";

  for (const auto &p : pages) {
    writeObjHead(p.pageObjNum);
    out << "<< /Type /Page /Parent " << pagesId << " 0 R "
        << "/MediaBox [0 0 612 792] "
        << "/Contents " << p.streamObjNum << " 0 R "
        << "/Resources << /Font << "
        << "/F1 " << fontF1Id << " 0 R "
        << "/F2 " << fontF2Id << " 0 R "
        << "/F3 " << fontF3Id << " 0 R "
        << "/F4 " << fontF4Id << " 0 R "
        << ">> >> >>\nendobj\n";

    writeObjHead(p.streamObjNum);
    out << "<< /Length " << p.streamData.size() << " >>\nstream\n"
        << p.streamData << "\nendstream\nendobj\n";
  }

  // xref table
  long xrefStart = (long)out.tellp();
  int totalObjs = (int)xref.size();
  out << "xref\n0 " << totalObjs << "\n";
  char buf[32];
  snprintf(buf, sizeof(buf), "%010d 65535 f \n", 0);
  out << buf;
  for (int i = 1; i < totalObjs; ++i) {
    snprintf(buf, sizeof(buf), "%010ld 00000 n \n", xref[i]);
    out << buf;
  }

  out << "trailer\n<< /Size " << totalObjs << " /Root " << catalogId
      << " 0 R >>\n";
  out << "startxref\n" << xrefStart << "\n%%EOF\n";

  out.close();
  return true;
}
