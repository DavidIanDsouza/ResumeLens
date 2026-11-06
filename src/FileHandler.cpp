#include "FileHandler.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>


/**
 * @brief Reads the file and returns its text content
 *
 * Detailed description: If the file is a PDF, it uses the external `pdftotext`
 * command-line utility to extract its text and throw it in a temporary text
 * file first. It then reads that temporary file into a buffer and cleans it up.
 * If it's a regular text file, it will read it normally using a file stream.
 *
 * @author David
 * @param filePath The path to the file we want to read
 * @return std::string the extracted text, or an empty string if it fails
 */
std::string FileHandler::readFile(const std::string &filePath) {
  if (filePath.length() >= 4 &&
      filePath.substr(filePath.length() - 4) == ".pdf") {
    std::string tempFile = "temp_extracted.txt";
#ifdef _WIN32
    std::string command =
        ".\\pdftotext.exe \"" + filePath + "\" \"" + tempFile + "\"";
#else
    // assuming standard pdftotext is installed for mac/linux
    std::string command = "pdftotext \"" + filePath + "\" \"" + tempFile + "\"";
#endif

    int result = std::system(command.c_str());
    if (result != 0) {
      std::cerr << "Error: Failed to process PDF file '" << filePath << "'."
                << std::endl;
      return "";
    }

    // Opens the temp file we just made
    std::ifstream file(tempFile);
    if (!file.is_open()) {
      std::cerr << "Error: Could not read extracted text from '" << filePath
                << "'." << std::endl;
      return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    // Deletes the temp file so we don't clutter up the folder
    std::remove(tempFile.c_str());

    return buffer.str();
  } else {
    std::ifstream file(filePath);

    // Checks if it actually opened
    if (!file.is_open()) {
      std::cerr << "Error: Could not open file at '" << filePath << "'."
                << std::endl;
      return "";
    }

    // Dumps the whole file into a stringstream
    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
  }
}
