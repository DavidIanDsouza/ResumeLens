#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <string>

/**
 * @class FileHandler
 * @brief Handles reading text from files, including PDF extraction.
 *
 * This class provides utility functions to read the contents of a file
 * into a string. It can automatically extract text from PDFs by invoking
 * the pdftotext executable.
 *
 * @author David
 */
class FileHandler {
public:
  /**
   * @brief Reads the entire contents of a file into a std::string.
   *
   * If the file is a PDF, it will use an external utility `pdftotext` to
   * extract its text into a temporary text file, read from that temporary file,
   * and then delete it. Otherwise, it reads the file normally as plain text
   * using an ifstream.
   *
   * @author David
   * @param filePath The path to the file to be read.
   * @return std::string The complete text contents of the file, or an empty
   * string if the file cannot be read.
   */
  static std::string readFile(const std::string &filePath);
};

#endif // FILE_HANDLER_H
