#include "JobMatch.h"
#include <algorithm>
#include <cmath>
#include <cctype>

/**
 * @brief Calculates how well a resume matches a job
 * 
 * Gets keywords from the job posting and sees if they're in the resume. 
 * Gives higher score for more important keywords.
 * 
 * @author Sean
 * @param jobKeywords words from job description and their weights
 * @param resumeText the actual resume text
 * @return double match percentage (rounded to 2 decimal places so it looks nice)
 */
double JobMatch::calculate(const std::unordered_map<std::string, int>& jobKeywords, const std::string& resumeText) {
    if (jobKeywords.empty()) return 0.0;

    double totalWeight = 0.0;
    double matchedWeight = 0.0;
    std::string lowerResume = toLower(resumeText);

    // check each keyword and add up weights 
    for (auto const& [keyword, weight] : jobKeywords) {
        totalWeight += weight;
        if (lowerResume.find(toLower(keyword)) != std::string::npos) {
            matchedWeight += weight;
        }
    }

    if (totalWeight == 0) return 0.0;

    // calculate the final score out of 100 
    double score = (matchedWeight / totalWeight) * 100.0;
        
    // round it off to 2 decimal places for the UI 
    return std::round(score * 100.0) / 100.0;
}

/**
 * @brief Helper to convert string to lowercase for comparisons 
 * 
 * @author Sean
 * @param data string we want to lowercase
 * @return std::string the lowercased version
 */
std::string JobMatch::toLower(std::string data) {
    std::transform(data.begin(), data.end(), data.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return data;
}
