#include "KeywordExtractor.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace {

/**
 * @brief List of common stop words to exclude from keyword extraction.
 * 
 * Expanded to prevent common conversational words from being falsely flagged 
 * as "keyword overuse".
 */
const std::unordered_set<std::string> STOP_WORDS = {
    "a", "about", "above", "after", "again", "against", "all", "am", "an",
    "and", "any", "are", "aren't", "as", "at", "be", "because", "been",
    "before", "being", "below", "between", "both", "but", "by", "can't",
    "cannot", "could", "couldn't", "did", "didn't", "do", "does", "doesn't",
    "doing", "don't", "down", "during", "each", "few", "for", "from", "further",
    "had", "hadn't", "has", "hasn't", "have", "haven't", "having", "he", "he'd",
    "he'll", "he's", "her", "here", "here's", "hers", "herself", "him",
    "himself", "his", "how", "how's", "i", "i'd", "i'll", "i'm", "i've", "if",
    "in", "into", "is", "isn't", "it", "it's", "its", "itself", "let's", "me",
    "more", "most", "mustn't", "my", "myself", "no", "nor", "not", "of", "off",
    "on", "once", "only", "or", "other", "ought", "our", "ours", "ourselves",
    "out", "over", "own", "same", "shan't", "she", "she'd", "she'll", "she's",
    "should", "shouldn't", "so", "some", "such", "than", "that", "that's",
    "the", "their", "theirs", "them", "themselves", "then", "there", "there's",
    "these", "they", "they'd", "they'll", "they're", "they've", "this", "those",
    "through", "to", "too", "under", "until", "up", "very", "was", "wasn't",
    "we", "we'd", "we'll", "we're", "we've", "were", "weren't", "what",
    "what's", "when", "when's", "where", "where's", "which", "while", "who",
    "who's", "whom", "why", "why's", "will", "with", "won't", "would",
    "wouldn't", "you", "you'd", "you'll", "you're", "you've", "your", "yours",
    "yourself", "yourselves", "use", "using", "used", "work", "working",
    "worked", "year", "years", "new", "also", "can", "may", "might", "one",
    "two", "three", "first", "second", "third", "inc", "ltd", "co", "corp",
    "based", "within", "without", "throughout", "among", "upon", "even",
    "still", "well", "ensure", "ensuring", "ensured", "make", "making", "made",
    "take", "taking", "took", "additional", "aug", "june", "sept", "week", "weeks", "month", "date",
    "don", "re", "non", "part", "level", "much", "just", "want", "get", "gets",
    "look", "looking", "every", "many", "done", "ways", "stuff", "please",
    "expected", "expect", "goes", "isn", "re", "around", "across",

    // Extraneous job description / resume filler terms
    "ability", "access", "account", "accounts", "action", "actions", "case",
    "cases", "common", "company", "companies", "description", "diagnose",
    "diagnosing", "duties", "either", "email", "focus", "high", "highly",
    "include", "includes", "including", "information", "issue", "issues", "job",
    "log", "logs", "manner", "meet", "preferred", "problem", "problems",
    "profile", "profiles", "provide", "provides", "providing", "provided",
    "request", "requests", "require", "required", "requirement", "requirements",
    "resolve", "resolving", "respond", "responsibility", "responsibilities",
    "role", "series", "set", "skill", "skills", "staff", "strong", "summary",
    "test", "testing", "time", "timely", "trend", "trends", "underlying",
    "understanding", "volume", "volumes", "walkthrough", "evaluate",
    "evaluating", "evaluation", "perform", "analyse", "analyzing", "analysis",
    "ms", "word", "abide", "accommodations", "affordable", "available", 
    "benefits", "bonus", "casual", "catered", "compensation",
    "confidential", "confidentiality", "countries", "culture", "dental", 
    "disability", "discriminatory", "duration", "eligible", "employee", 
    "employees", "health", "insurance", "lunches", "medical", "meetings", 
    "office", "offices", "outings", "package", "paid", "pay", "recreational", 
    "salary", "savings", "swag", "united", "states", "visit", "big", "cost", 
    "chance", "class", "environment", "environments", "excellent", "fast", 
    "future", "hour", "hourly", "hours", "large", "plan", "program"};

/**
 * @brief Checks if a character is a letter or number.
 * 
 * @authors Rahul, Anish, David, Sean
 * @param c The character to check.
 * @returns true If the character is alphanumeric.
 * @returns false Otherwise.
 */
bool isWordChar(char c) { return std::isalnum(static_cast<unsigned char>(c)); }

/**
 * @brief Gets the next word from the sequence and advances the index.
 * 
 * @authors Rahul, Anish, David, Sean
 * @param text The sequence of characters to extract from.
 * @param pos The current index to extract the next word at.
 * @returns std::string The extracted word.
 */
std::string getNextWord(const std::string &text, size_t &pos) {
  // skip the junk characters
  while (pos < text.size() && !isWordChar(text[pos])) {
    ++pos;
  }
  if (pos >= text.size()) {
    return "";
  }
  size_t start = pos;
  while (pos < text.size() && isWordChar(text[pos])) {
    ++pos;
  }
  return text.substr(start, pos - start);
}

/**
 * @brief Helper to convert a string to lowercase.
 * 
 * @authors Rahul, Anish, David, Sean
 * @param s The string to turn lowercase.
 * @returns std::string The lowercased string.
 */
std::string toLower(const std::string &s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

// basic word stemming to merge plurals (e.g. careers -> career)
std::string basicStem(const std::string &word) {
  if (word.length() <= 3) return word;

  if (word.length() > 4 && word.substr(word.length() - 3) == "ies") {
    if (word != "series" && word != "species") {
      return word.substr(0, word.length() - 3) + "y";
    }
  }

  if (word.length() > 4 && word.substr(word.length() - 2) == "es") {
    char prev = word[word.length() - 3];
    if (prev == 's' || prev == 'x' || prev == 'z' || prev == 'h') {
      return word.substr(0, word.length() - 2);
    }
  }

  if (word.back() == 's' && word[word.length() - 2] != 's' && word[word.length() - 2] != 'u' && word[word.length() - 2] != 'i') {
    return word.substr(0, word.length() - 1);
  }

  return word;
}

} // namespace

/**
 * @brief Gets keywords and their counts from the text
 *
 * Goes through the string (like a resume or job posting), ignores common words,
 * and figures out how many times each important word is used.
 * Everything gets lowercased so it is easier to compare later.
 *
 * @authors Rahul, Anish, David, Sean
 * @param text the input string to parse
 * @returns std::unordered_map<std::string, int> map of word counts
 */
std::unordered_map<std::string, int>
KeywordExtractor::extract(const std::string &text, const std::string &ignoreWords) {
  std::unordered_map<std::string, int> frequencyMap;

  if (text.empty()) {
    return frequencyMap;
  }

  // Parse ignore words into a set
  std::unordered_set<std::string> customIgnore;
  if (!ignoreWords.empty()) {
    size_t p = 0;
    while (p < ignoreWords.size()) {
      std::string w = getNextWord(ignoreWords, p);
      if (w.empty()) break;
      customIgnore.insert(toLower(w));
    }
  }

  size_t pos = 0;
  while (pos < text.size()) {
    std::string word = getNextWord(text, pos);
    if (word.empty()) {
      break;
    }

    if (std::isdigit(static_cast<unsigned char>(word[0]))) {
        continue;
    }

    // lowercase so we don't count "Word" and "word" differently
    std::string normalized = toLower(word);

    // ignore single chars cause they're usually just noise
    if (normalized.length() < 2) {
      continue;
    }

    // ignore stop words or custom ignore words
    if (STOP_WORDS.count(normalized) || customIgnore.count(normalized)) {
      continue;
    }

    // handle plurals (careers -> career)
    normalized = basicStem(normalized);

    // check again in case stemmed version is a stop word or custom ignore
    if (STOP_WORDS.count(normalized) || customIgnore.count(normalized)) {
      continue;
    }

    ++frequencyMap[normalized];
  }

  return frequencyMap;
}
