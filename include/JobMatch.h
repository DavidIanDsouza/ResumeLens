#ifndef JOB_MATCH_H
#define JOB_MATCH_H

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class JobMatch
 * @brief Calculates the alignment between a resume and a job description.
 *
 * @author Sean
 * This class compares keywords extracted from a job description against
 * the text of a resume to produce an ATS (Applicant Tracking System) match score.
 */
class JobMatch {
public:
  /**
   * @brief Calculates the job match percentage.
   * 
   * @author Sean
   * @param jobKeywords A map of extracted keywords and their frequencies from the job description.
   * @param resumeText The full text of the resume.
   * @return double The match percentage score (0.0 to 100.0).
   */
  static double
  calculate(const std::unordered_map<std::string, int> &jobKeywords,
            const std::string &resumeText);

private:
  /**
   * @brief Helper function to convert a string to lowercase.
   * 
   * @author Sean
   * @param data The string to convert.
   * @return std::string The lowercase string.
   */
  static std::string toLower(std::string data);
};

#endif // JOB_MATCH_H
