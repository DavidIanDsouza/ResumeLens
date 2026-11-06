#ifndef KEYWORD_ALERTS_H
#define KEYWORD_ALERTS_H

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class KeywordAlerts
 * @brief Analyzes a resume for potential keyword stuffing.
 *
 * This class checks the frequency of keywords in a resume against
 * expected thresholds to warn the user if a specific skill or word
 * is used unnaturally often.
 * 
 * @author Anish
 */
class KeywordAlerts {
public:
  /**
   * @brief Checks the resume for overused keywords.
   * 
   * @author Anish
   * @param jobKeywords A map of expected keywords.
   * @param resumeText The full text of the resume.
   * @returns std::vector<std::string> A list of alert messages regarding overused keywords.
   */
  static std::vector<std::string>
  check(const std::unordered_map<std::string, int> &jobKeywords,
        const std::string &resumeText);
};

#endif // KEYWORD_ALERTS_H
