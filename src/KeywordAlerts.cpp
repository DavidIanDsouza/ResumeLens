#include "KeywordAlerts.h"
#include "KeywordExtractor.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

/**
 * @brief Finds words that are used way too much in the resume
 * 
 * Checks how many times words show up. If a job keyword is used like 5+ times,
 * or a regular word is used too much, we warn the user so it doesn't look like
 * they're just keyword stuffing.
 * 
 * @author Anish
 * @param jobKeywords map of keywords from the job posting
 * @param resumeText text from the resume
 * @returns std::vector<std::string> words that were overused
 */
std::vector<std::string>
KeywordAlerts::check(const std::unordered_map<std::string, int> &jobKeywords,
                     const std::string &resumeText) {
  std::vector<std::string> alerts;

  // 1. first get counts of all words in the resume
  auto resumeKeywords = KeywordExtractor::extract(resumeText);

  // 2. set up the thresholds for what counts as "overuse"
  const int JOB_KEYWORD_THRESHOLD = 8;
  const int NORMAL_KEYWORD_THRESHOLD = 10;

  // 2.5: need a lowercase version of job keywords since extractor lowercases everything
  std::unordered_set<std::string> lowerJobKeywords;
  for (const auto &[keyword, weight] : jobKeywords) {
    std::string lowerKeyword = keyword;
    std::transform(lowerKeyword.begin(), lowerKeyword.end(),
                   lowerKeyword.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    lowerJobKeywords.insert(lowerKeyword);
  }

  // 3. loop through all the resume keywords
  for (const auto &[word, count] : resumeKeywords) {

    // 4. see if word is a job keyword
    if (lowerJobKeywords.find(word) != lowerJobKeywords.end()) {

      // 5. flag it if used more than job threshold
      if (count > JOB_KEYWORD_THRESHOLD) {
        alerts.push_back(word);
      }
    } else {
      // 6. not a job keyword, so use the lower threshold
      if (count > NORMAL_KEYWORD_THRESHOLD) {
        alerts.push_back(word);
      }
    }
  }

  return alerts;
}
