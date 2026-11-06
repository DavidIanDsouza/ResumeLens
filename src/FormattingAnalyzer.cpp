#include "FormattingAnalyzer.h"
#include <algorithm>
#include <cctype>
#include <sstream>


/**
 * @brief Checks if the resume uses consistent capitalization for bullets
 *
 * Finds bullet points (-, *, etc) and checks if the first word is capitalized.
 * It's inconsistent if some start with uppercase and others don't.
 *
 * @author David, Anish, Sean
 * @param text the entire resume as a string
 * @returns FormattingResult result obj with score and outliers
 */

FormattingResult
FormattingAnalyzer::checkCapitalizationConsistency(const std::string &text) {
  std::istringstream stream(text);
  std::string line;
  int bulletCount = 0;
  int lineNumber = 0;

  std::vector<std::pair<int, std::string>> capitalizedLines;
  std::vector<std::pair<int, std::string>> uncapitalizedLines;

  while (std::getline(stream, line)) {
    lineNumber++;
    // skip spaces at the start of the line to find the bullet
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue; // just skip empty lines

    bool isBullet = false;
    size_t textStart = std::string::npos;
    
    if (start < line.length()) {
        char firstChar = line[start];
        bool isSymbol = (firstChar == '-' || firstChar == '*' || firstChar == '+' || 
                         firstChar == '>' || firstChar == '~' || 
                         static_cast<unsigned char>(firstChar) > 127);
        
        if (isSymbol) {
             size_t firstAlnum = std::string::npos;
             for (size_t i = start; i < line.length(); i++) {
                  if (std::isalnum(static_cast<unsigned char>(line[i]))) {
                      firstAlnum = i;
                      break;
                  }
             }
             if (firstAlnum != std::string::npos && firstAlnum > start) {
                  isBullet = true;
                  textStart = firstAlnum;
             }
        }
    }

    if (isBullet) {
      bulletCount++;

      // get the first actual character after the bullet
      std::string cleanText = line.substr(textStart);
      if (std::isupper(static_cast<unsigned char>(line[textStart]))) {
        capitalizedLines.push_back({lineNumber, cleanText});
      } else {
        uncapitalizedLines.push_back({lineNumber, cleanText});
      }
    }
  }

  FormattingResult result;
  if (bulletCount == 0) {
    result.isConsistent = true;
    result.consistencyScore = 100.0;
    return result;
  }

  int capCount = capitalizedLines.size();

  result.consistencyScore = ((double)capCount / bulletCount) * 100.0;

  // It's only 100% Good if everything is capitalized
  if (capCount == bulletCount) {
    result.isConsistent = true;
    return result;
  }

  result.isConsistent = false;
  result.outliers =
      uncapitalizedLines; // return all lines missing capitalization

  return result;
}

/**
 * @brief Checks if bullet points use consistent ending punctuation
 *
 * Finds bullet points and checks if the line ends with a period, comma, or
 * semicolon. Bad formatting if some lines have periods and others don't end
 * with anything.
 *
 * @author David, Anish, Sean
 * @param text the whole resume text
 * @returns FormattingResult results including score and bad lines
 */
FormattingResult
FormattingAnalyzer::checkPunctuationConsistency(const std::string &text) {
  std::istringstream stream(text);
  std::string line;
  int bulletCount = 0;
  int lineNumber = 0;

  std::vector<std::pair<int, std::string>> punctuatedLines;
  std::vector<std::pair<int, std::string>> unpunctuatedLines;

  while (std::getline(stream, line)) {
    lineNumber++;
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue; // Empty line

    bool isBullet = false;
    size_t textStart = std::string::npos;
    
    if (start < line.length()) {
        char firstChar = line[start];
        bool isSymbol = (firstChar == '-' || firstChar == '*' || firstChar == '+' || 
                         firstChar == '>' || firstChar == '~' || 
                         static_cast<unsigned char>(firstChar) > 127);
        
        if (isSymbol) {
             size_t firstAlnum = std::string::npos;
             for (size_t i = start; i < line.length(); i++) {
                  if (std::isalnum(static_cast<unsigned char>(line[i]))) {
                      firstAlnum = i;
                      break;
                  }
             }
             if (firstAlnum != std::string::npos && firstAlnum > start) {
                  isBullet = true;
                  textStart = firstAlnum;
             }
        }
    }

    if (isBullet) {
      bulletCount++;

      std::string cleanText = line.substr(textStart);

      // find the last actual char on the line (ignoring spaces/newlines)
      size_t end = line.find_last_not_of(" \t\r\n");
      if (end != std::string::npos) {
        char lastChar = line[end];
        if (lastChar == '.' || lastChar == ';' || lastChar == ',') {
          punctuatedLines.push_back({lineNumber, cleanText});
        } else {
          unpunctuatedLines.push_back({lineNumber, cleanText});
        }
      } else {
        unpunctuatedLines.push_back({lineNumber, cleanText});
      }
    }
  }

  FormattingResult result;
  if (bulletCount == 0) {
    result.isConsistent = true;
    result.consistencyScore = 100.0;
    return result;
  }

  int punctuatedBulletCount = punctuatedLines.size();
  int unpunctuatedBulletCount = unpunctuatedLines.size();

  result.consistencyScore =
      (std::max(punctuatedBulletCount, unpunctuatedBulletCount) /
       (double)bulletCount) *
      100.0;

  // perfectly consistent if all have punctuation or all don't
  if (punctuatedBulletCount == bulletCount ||
      unpunctuatedBulletCount == bulletCount) {
    result.isConsistent = true;
    return result;
  }

  result.isConsistent = false;
  double puncRatio = (double)punctuatedBulletCount / bulletCount;
  double unpuncRatio = (double)unpunctuatedBulletCount / bulletCount;

  if (puncRatio >= 0.8) {
    result.outliers = unpunctuatedLines;
  } else if (unpuncRatio >= 0.8) {
    result.outliers = punctuatedLines;
  } else {
    result.outliers.push_back(
        {-1, "Consistency is below 80%. No clear majority style."});
  }

  return result;
}
