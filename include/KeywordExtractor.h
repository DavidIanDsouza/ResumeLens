#ifndef KEYWORD_EXTRACTOR_H
#define KEYWORD_EXTRACTOR_H

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class KeywordExtractor
 * @brief Extracts essential keywords from a job description.
 *
 * This class processes raw text from a job description, filters out
 * common stop words, and identifies significant technical and soft skills.
 * 
 * @author Rahul, Anish
 */
class KeywordExtractor {
public:
  /**
   * @brief Extracts keywords and their frequencies from text.
   * 
   * @author Rahul, Anish
   * @param text The job description text to analyze.
   * @returns std::unordered_map<std::string, int> A map consisting of extracted keywords and their observation counts.
   */
  static std::unordered_map<std::string, int> extract(const std::string &text, const std::string &ignoreWords = "");
};

#endif // KEYWORD_EXTRACTOR_H
