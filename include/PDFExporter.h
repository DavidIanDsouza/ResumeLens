#ifndef PDF_EXPORTER_H
#define PDF_EXPORTER_H

#include <string>

/**
 * @class PDFExporter
 * @brief Handles generation of raw PDF text documents natively using standard
 * C++.
 *
 * Detailed description: It is capable of taking plain text strings, properly
 * validating them, paginating them by measuring string lengths and newline
 * blocks, and finally serializing the objects, cross-reference tables, and
 * stream dictionaries into a standard PDF-1.4 file output.
 *
 * @author David & Pal
 */
class PDFExporter {
public:
  /**
   * @brief Exports plain text into a newly generated PDF.
   *
   * Detailed description: Evaluates line types, word-wraps text, and builds a
   * cross-referenced PDF document fully from scratch using PDF primitive
   * streams.
   *
   * @author David & Pal
   * @param outputPath The path where the PDF will be saved.
   * @param rawText The extracted raw text string to convert.
   * @return true if write operation succeeds.
   * @return false if write operation fails (e.g., IO error).
   */
  static bool exportToPDF(const std::string &outputPath,
                          const std::string &rawText);
};

#endif // PDF_EXPORTER_H
