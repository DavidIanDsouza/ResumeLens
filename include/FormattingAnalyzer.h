#ifndef FORMATTING_ANALYZER_H
#define FORMATTING_ANALYZER_H

#include <string>
#include <vector>
#include <utility>

/**
 * @class FormattingAnalyzer
 * @brief Analyzes the consistency of formatting in a resume.
 *
 * This class provides static methods to check if bullet points in the
 * resume are formatted consistently regarding capitalization and punctuation.
 * 
 * @author David, Anish, Sean
 */

/**
 * @struct FormattingResult
 * @brief Holds the results of a formatting consistency check
 * 
 * Provides details about whether the resume formatting meets the 80% majority 
 * rule, its consistency percentage, and a list of structural outliers.
 */
struct FormattingResult {
  bool isConsistent; /**< true if it passes the 80% majority threshold */
  double consistencyScore; /**< the percentage of majority consistency */
  std::vector<std::pair<int, std::string>> outliers; /**< lines/issues that didn't match the majority */
};

class FormattingAnalyzer {
public:
    /**
     * @brief Analyzes if the bullet points in the resume consistently start with a capital letter.
     * 
     * @param text The resume content to analyze.
     * @return FormattingResult detailing line-specific capitalization formatting.
     */
    static FormattingResult checkCapitalizationConsistency(const std::string& text);

    /**
     * @brief Analyzes if the bullet points in the resume consistently end with punctuation (like periods).
     * 
     * @param text The resume content to analyze.
     * @return FormattingResult containing boolean success and the outlier strings.
     */
    static FormattingResult checkPunctuationConsistency(const std::string& text);
};

#endif // FORMATTING_ANALYZER_H
