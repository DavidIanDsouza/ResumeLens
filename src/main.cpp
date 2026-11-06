#ifdef _WIN32
#include <windows.h>
// commdlg.h requires windows.h to be included first
#include <commdlg.h>
#endif

#include "FileHandler.h"
#include "FormattingAnalyzer.h"
#include "JobMatch.h"
#include "KeywordAlerts.h"
#include "KeywordExtractor.h"
#include "PDFExporter.h"
#include "ResumeAI.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Helper function to convert a string to lowercase.
 *
 * This function iterates through each character
 * of the provided string and uses std::tolower to convert it to its
 * lowercase equivalent. It's useful for case-insensitive comparisons
 * such as keyword matching.
 *
 * @author David
 * @param data The input string to be converted.
 * @return std::string The lowercase version of the input string.
 */
static std::string toLowerString(std::string data) {
  std::transform(data.begin(), data.end(), data.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return data;
}

/**
 * @brief Opens native file dialog on Windows/Mac, or asks for terminal input on
 * other platforms.
 *
 * Invokes OS-specific APIs to open a file selector GUI.
 * On Windows, it uses the comdlg32 library's GetOpenFileNameA.
 * On macOS, it uses AppleScript (osascript) to prompt the user.
 * On Linux or other platforms, it falls back to a terminal prompt.
 *
 * @author David
 * @param title The title of the dialog window or prompt.
 * @param filter A null-separated list of file extensions used to filter the
 * file types in the dialog.
 * @return std::string The absolute path to the selected file, or an empty
 * string if cancelled.
 */
static std::string getOpenFilePath(const char *title, const char *filter) {
#ifdef _WIN32
  HMODULE hComdlg32 = LoadLibraryA("comdlg32.dll");
  if (hComdlg32) {
    typedef BOOL(WINAPI * PGETOPENFILENAMEA)(LPOPENFILENAMEA);
    PGETOPENFILENAMEA pGetOpenFileNameA =
        (PGETOPENFILENAMEA)GetProcAddress(hComdlg32, "GetOpenFileNameA");

    if (pGetOpenFileNameA) {
      OPENFILENAMEA ofn;
      char szFile[260] = {0};
      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = NULL;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = filter;
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = NULL;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
      ofn.lpstrTitle = title;

      if (pGetOpenFileNameA(&ofn) == TRUE) {
        FreeLibrary(hComdlg32);
        return std::string(ofn.lpstrFile);
      }
    }
    FreeLibrary(hComdlg32);
  }
#elif defined(__APPLE__)
  std::string command =
      "osascript -e 'POSIX path of (choose file with prompt \"" +
      std::string(title) + "\" of type {\"pdf\", \"txt\", \"text\"})'";
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe)
    return "";
  char buffer[128];
  std::string result = "";
  while (!feof(pipe)) {
    if (fgets(buffer, 128, pipe) != nullptr)
      result += buffer;
  }
  pclose(pipe);

  // Remove trailing newline
  if (!result.empty() && result[result.length() - 1] == '\n') {
    result.erase(result.length() - 1);
  }
  return result;
#else
  std::cout << "\n[" << title << "]" << std::endl;
  std::cout << "Enter file path (or leave empty to go back): ";
  std::string path;
  std::getline(std::cin, path);
  return path;
#endif
  return "";
}

/**
 * @brief Opens native save file dialog on Windows/Mac, or asks for terminal
 * input on other platforms.
 *
 * Invokes OS-specific APIs to open a "save file" GUI dialog.
 * On Windows, it uses the GetSaveFileNameA function from the comdlg32 library.
 * On macOS, it uses an AppleScript to prompt for a save destination.
 * On other platforms, it prompts the user in standard input.
 *
 * @author David
 * @param title The title of the save dialog.
 * @param filter The file extension filter string.
 * @return std::string The path chosen by the user to save the file to, or an
 * empty string if cancelled.
 */
static std::string getSaveFilePath(const char *title, const char *filter) {
#ifdef _WIN32
  HMODULE hComdlg32 = LoadLibraryA("comdlg32.dll");
  if (hComdlg32) {
    typedef BOOL(WINAPI * PGETSAVEFILENAMEA)(LPOPENFILENAMEA);
    PGETSAVEFILENAMEA pGetSaveFileNameA =
        (PGETSAVEFILENAMEA)GetProcAddress(hComdlg32, "GetSaveFileNameA");

    if (pGetSaveFileNameA) {
      OPENFILENAMEA ofn;
      char szFile[260] = {0};
      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = NULL;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = filter;
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = NULL;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
      ofn.lpstrTitle = title;
      ofn.lpstrDefExt = "pdf";

      if (pGetSaveFileNameA(&ofn) == TRUE) {
        FreeLibrary(hComdlg32);
        return std::string(ofn.lpstrFile);
      }
    }
    FreeLibrary(hComdlg32);
  }
#elif defined(__APPLE__) // macOS
  std::string command =
      "osascript -e 'POSIX path of (choose file name with prompt \"" +
      std::string(title) + "\" default name \"EditedResume.pdf\")'";
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe)
    return "";
  char buffer[128];
  std::string result = "";
  while (!feof(pipe)) {
    if (fgets(buffer, 128, pipe) != nullptr)
      result += buffer;
  }
  pclose(pipe);
  if (!result.empty() && result[result.length() - 1] == '\n')
    result.erase(result.length() - 1);
  return result;
#else
  std::cout << "\n[" << title << "]" << std::endl;
  std::cout << "Enter save file path (or leave empty to go back): ";
  std::string path;
  std::getline(std::cin, path);
  return path;
#endif
  return "";
}

/**
 * @brief Checks if the user likely swapped their resume and job description.
 *
 * Since users might upload their resume in the job description
 * slot and vice versa, this helper counts common resume keywords vs common JD
 * keywords. If the provided resume text has more JD keywords, and the JD text
 * has more resume keywords, it returns true indicating they are probably
 * swapped.
 *
 * @author David
 * @param resumeText The text content of the uploaded resume file.
 * @param jdText The text content of the uploaded job description file.
 * @return true If the files appear to be swapped.
 * @return false If the files seem correct or indistinguishable.
 */
static bool isLikelySwapped(const std::string &resumeText,
                            const std::string &jdText) {
  auto countKeywords = [](const std::string &text,
                          const std::vector<std::string> &words) {
    int count = 0;
    std::string lowerText = toLowerString(text);
    for (const auto &w : words) {
      if (lowerText.find(w) != std::string::npos) {
        count++;
      }
    }
    return count;
  };

  std::vector<std::string> resumeKeywords = {
      "education", "coursework", "gpa",      "bachelor",
      "master",    "university", "academic", "projects"};
  std::vector<std::string> jdKeywords = {"responsibilities",
                                         "requirements",
                                         "qualifications",
                                         "preferred qualifications",
                                         "equal opportunity",
                                         "benefits",
                                         "apply",
                                         "what you'll do",
                                         "what you will do",
                                         "role"};

  int rAsResume = countKeywords(resumeText, resumeKeywords);
  int rAsJd = countKeywords(resumeText, jdKeywords);

  int jAsResume = countKeywords(jdText, resumeKeywords);
  int jAsJd = countKeywords(jdText, jdKeywords);

  if ((rAsJd > rAsResume) && (jAsResume > jAsJd)) {
    return true;
  }
  return false;
}

/**
 * @brief Loads the chosen files into text variables and performs validation.
 *
 * Ensures that both a resume file and a job description
 * file have been chosen by the user. If they have, it reads them into the
 * provided string references. It also verifies that they aren't empty, and runs
 * our isLikelySwapped helper to prompt the user if they uploaded the files
 * backwards.
 *
 * @author David
 * @param resumeFile Reference to the resume file path string. Will be modified
 * if swapped.
 * @param jdFile Reference to the job description file path string. Will be
 * modified if swapped.
 * @param resumeText Reference to the string where the read resume text will be
 * stored.
 * @param jobDescText Reference to the string where the read JD text will be
 * stored.
 * @return true If both files were successfully loaded and validated.
 * @return false If file paths are missing or reading fails.
 */
static bool loadAndCheckFiles(std::string &resumeFile, std::string &jdFile,
                              std::string &resumeText,
                              std::string &jobDescText) {
  if (resumeFile.empty() || jdFile.empty()) {
    std::cout << "\n[!] Error: Please select both a Resume and a Job "
                 "Description file first."
              << std::endl;
    return false;
  }
  // Read files
  resumeText = FileHandler::readFile(resumeFile);
  jobDescText = FileHandler::readFile(jdFile);
  // Check if files are empty
  if (resumeText.empty() || jobDescText.empty()) {
    std::cout << "[!] Error: Could not read files. Ensure they are valid PDF "
                 "or TXT files."
              << std::endl;
    return false;
  }
  // Check if files are swapped
  if (isLikelySwapped(resumeText, jobDescText)) {
    std::cout << "\n[!] Warning: It looks like you may have swapped your "
                 "Resume and Job Description files."
              << std::endl;
    std::cout
        << "    Your Resume looks more like a Job Description and vice versa."
        << std::endl;
    std::cout << "    Would you like to swap them automatically? (y/n): ";
    std::string ans;
    std::getline(std::cin, ans);
    if (ans == "y" || ans == "Y") {
      std::swap(resumeFile, jdFile);
      std::swap(resumeText, jobDescText);
      std::cout << "[+] Files swapped successfully.\n";
    }
  }
  return true;
}

/**
 * @brief Extracts the base filename from a full file path.
 *
 * Detailed description: Takes a string representing an absolute or relative
 * file path and searches for the last directory separator (either forward slash
 * or backslash). It returns just the file name at the end of the path.
 *
 * @author David
 * @param path The full or relative file path string.
 * @return std::string The extracted base file name.
 */
static std::string getBaseName(const std::string &path) {
  size_t pos = path.find_last_of("/\\");
  return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

/**
 * @brief Primary execution function for the Resume Lens CLI.
 *
 * Implements a core menu loop allowing standard users
 * to repeatedly choose options: loading resumes, loading job descriptions,
 * running the analysis (formatting and keywords matches), doing a manual AI
 * edit process, or using the AI coach (Ollama/Groq). The loop continues running
 * until the user selects choice "6" to Exit.
 *
 * @author David
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return int Exit status (0 for successful execution).
 */
int main(int argc, char *argv[]) {
  std::string resumeFile = "";
  std::string jdFile = "";
  std::string companyIgnoreWords = "";

  while (true) {
    std::cout << "\n=========================================" << std::endl;
    std::cout << "          RESUME LENS - MAIN MENU        " << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << " [1] Browse Resume File (Current: "
              << (resumeFile.empty() ? "None" : resumeFile) << ")" << std::endl;
    std::cout << " [2] Browse Job Description (Current: "
              << (jdFile.empty() ? "None" : jdFile) << ")" << std::endl;
    std::cout << " [3] Analyze Resume" << std::endl;
    std::cout << " [4] Manual Edit & PDF Export" << std::endl;
    std::cout << " [5] AI Resume Coach (Ollama -- optional Groq)" << std::endl;
    std::cout << " [6] Exit" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Please select an option: ";

    std::string choiceStr;
    if (!std::getline(std::cin, choiceStr)) {
      break; // EOF or input error
    }

    if (choiceStr == "1") {
      std::string temp = getOpenFilePath(
          "Select Resume", "PDF and TXT Files\0*.pdf;*.txt\0PDF "
                           "Files\0*.pdf\0Text Files\0*.txt\0All Files\0*.*\0");
      if (!temp.empty())
        resumeFile = temp;
    } else if (choiceStr == "2") {
      std::string temp =
          getOpenFilePath("Select Job Description",
                          "PDF and TXT Files\0*.pdf;*.txt\0PDF "
                          "Files\0*.pdf\0Text Files\0*.txt\0All Files\0*.*\0");
      if (!temp.empty()) {
        jdFile = temp;
        std::cout << "\nEnter company name or other words to ignore (or press Enter to skip): ";
        std::getline(std::cin, companyIgnoreWords);
      }
    } else if (choiceStr == "3") {
      std::string resumeText, jobDescText;
      if (!loadAndCheckFiles(resumeFile, jdFile, resumeText, jobDescText)) {
        continue;
      }

      std::cout << "\nAnalyzing... Please wait." << std::endl;

      auto jobKeywords = KeywordExtractor::extract(jobDescText, companyIgnoreWords);

      // Build matched and missing lists
      std::vector<std::pair<std::string, int>> matchedKeywords;
      std::vector<std::pair<std::string, int>> missingKeywords;
      std::string lowerResume = toLowerString(resumeText);
      // Check for matched keywords
      for (auto const &[keyword, weight] : jobKeywords) {
        if (lowerResume.find(toLowerString(keyword)) != std::string::npos) {
          matchedKeywords.push_back({keyword, weight});
        } else {
          missingKeywords.push_back({keyword, weight});
        }
      }

      // Standardize output sorting by frequency (descending) then alphabetical
      auto cmp = [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
          if (a.second != b.second) return a.second > b.second;
          return a.first < b.first;
      };

      std::sort(matchedKeywords.begin(), matchedKeywords.end(), cmp);
      std::sort(missingKeywords.begin(), missingKeywords.end(), cmp);

      // Keep only top 20
      if (matchedKeywords.size() > 20) matchedKeywords.resize(20);
      if (missingKeywords.size() > 20) missingKeywords.resize(20);

      double matchScore = JobMatch::calculate(jobKeywords, resumeText);

      std::cout << "\n-------------- RESULTS --------------\n" << std::endl;
      std::cout << "Job Match Score: " << matchScore << "%" << std::endl;
      // Print matched keywords
      std::cout << "\n[+] Matched Keywords (Top 20)" << std::endl;
      if (matchedKeywords.empty())
        std::cout << "  (None found)" << std::endl;
      for (const auto &kv : matchedKeywords) {
        std::cout << "  + " << kv.first << " (" << kv.second << ")" << std::endl;
      }
      // Print missing keywords
      std::cout << "\n[-] Missing Keywords (Top 20)" << std::endl;
      if (missingKeywords.empty())
        std::cout << "  (None missing!)" << std::endl;
      for (const auto &kv : missingKeywords) {
        std::cout << "  - " << kv.first << " (" << kv.second << ")" << std::endl;
      }
      // Check formatting consistency
      FormattingResult capResult =
          FormattingAnalyzer::checkCapitalizationConsistency(resumeText);
      FormattingResult puncResult =
          FormattingAnalyzer::checkPunctuationConsistency(resumeText);
      std::cout << "\n--- Formatting Analysis ---" << std::endl;

      std::cout << "Capitalization Consistency [Score: "
                << capResult.consistencyScore << "%]: "
                << (capResult.isConsistent ? "Good" : "Needs Improvement")
                << std::endl;
      // Print capitalization outliers
      if (!capResult.isConsistent && !capResult.outliers.empty()) {
        std::cout << "  [!] Capitalization Outliers / Issues:" << std::endl;
        for (const auto &out : capResult.outliers) {
          int lineNum = out.first;
          std::string textLine = out.second;
          size_t first = textLine.find_first_not_of(" \t");
          size_t last = textLine.find_last_not_of(" \t\r\n");
          if (first != std::string::npos && last != std::string::npos) {
            textLine = textLine.substr(first, last - first + 1);
          }
          if (lineNum == -1) {
            std::cout << "      - " << textLine << std::endl;
          } else {
            std::cout << "      - Line " << lineNum << ": " << textLine
                      << std::endl;
          }
        }
      }

      std::cout << "Punctuation Consistency [Score: "
                << puncResult.consistencyScore << "%]: "
                << (puncResult.isConsistent ? "Good" : "Needs Improvement")
                << std::endl;

      if (!puncResult.isConsistent && !puncResult.outliers.empty()) {
        std::cout << "  [!] Punctuation Outliers / Issues:" << std::endl;
        for (const auto &out : puncResult.outliers) {
          int lineNum = out.first;
          std::string textLine = out.second;
          size_t first = textLine.find_first_not_of(" \t");
          size_t last = textLine.find_last_not_of(" \t\r\n");
          if (first != std::string::npos && last != std::string::npos) {
            textLine = textLine.substr(first, last - first + 1);
          }
          if (lineNum == -1) {
            std::cout << "      - " << textLine << std::endl;
          } else {
            std::cout << "      - Line " << lineNum << ": " << textLine
                      << std::endl;
          }
        }
      }
      // Check for overuse of keywords
      auto overuseAlerts = KeywordAlerts::check(jobKeywords, resumeText);
      std::cout << "\n--- Keyword Overuse Alerts ---" << std::endl;
      if (overuseAlerts.empty()) {
        std::cout << "No keywords are overused." << std::endl;
      } else {
        for (const auto &alert : overuseAlerts) {
          std::cout << "! " << alert << std::endl;
        }
      }

      std::cout << "\n-------------------------------------" << std::endl;

      std::cout << "\nDo you want to save a PDF version of the result? (y/n): ";
      std::string saveAns;
      std::getline(std::cin, saveAns);
      if (saveAns == "y" || saveAns == "Y") {
        std::string savePath = getSaveFilePath(
            "Save PDF As", "PDF Files\0*.pdf\0All Files\0*.*\0");
        if (!savePath.empty()) {
          // Ensure extension is .pdf
          if (savePath.length() < 4 ||
              toLowerString(savePath.substr(savePath.length() - 4)) != ".pdf") {
            savePath += ".pdf";
          }

          std::ostringstream report;
          report << "-------------- RESULTS --------------\n\n";
          report << "Target Resume: " << getBaseName(resumeFile) << "\n";
          report << "Target Job Description: " << getBaseName(jdFile) << "\n\n";
          report << "Job Match Score: " << matchScore << "%\n\n";

          report << "[+] Matched Keywords (Top 20)\n";
          if (matchedKeywords.empty())
            report << "  (None found)\n";
          for (const auto &kv : matchedKeywords)
            report << "  - " << kv.first << " (" << kv.second << ")\n"; // PDFExporter uses - for bullets

          report << "\n[-] Missing Keywords (Top 20)\n";
          if (missingKeywords.empty())
            report << "  (None missing!)\n";
          for (const auto &kv : missingKeywords)
            report << "  - " << kv.first << " (" << kv.second << ")\n";

          report << "\n--- Formatting Analysis ---\n";
          report << "Capitalization Consistency [Score: "
                 << capResult.consistencyScore << "%]: "
                 << (capResult.isConsistent ? "Good" : "Needs Improvement")
                 << "\n";
          if (!capResult.isConsistent && !capResult.outliers.empty()) {
            report << "  [!] Capitalization Outliers / Issues:\n";
            for (const auto &out : capResult.outliers) {
              int lineNum = out.first;
              std::string textLine = out.second;
              size_t first = textLine.find_first_not_of(" \t");
              size_t last = textLine.find_last_not_of(" \t\r\n");
              if (first != std::string::npos && last != std::string::npos)
                textLine = textLine.substr(first, last - first + 1);
              if (lineNum == -1)
                report << "      - " << textLine << "\n";
              else
                report << "      - Line " << lineNum << ": " << textLine
                       << "\n";
            }
          }

          report << "\nPunctuation Consistency [Score: "
                 << puncResult.consistencyScore << "%]: "
                 << (puncResult.isConsistent ? "Good" : "Needs Improvement")
                 << "\n";
          if (!puncResult.isConsistent && !puncResult.outliers.empty()) {
            report << "  [!] Punctuation Outliers / Issues:\n";
            for (const auto &out : puncResult.outliers) {
              int lineNum = out.first;
              std::string textLine = out.second;
              size_t first = textLine.find_first_not_of(" \t");
              size_t last = textLine.find_last_not_of(" \t\r\n");
              if (first != std::string::npos && last != std::string::npos)
                textLine = textLine.substr(first, last - first + 1);
              if (lineNum == -1)
                report << "      - " << textLine << "\n";
              else
                report << "      - Line " << lineNum << ": " << textLine
                       << "\n";
            }
          }

          report << "\n--- Keyword Overuse Alerts ---\n";
          if (overuseAlerts.empty()) {
            report << "No keywords are overused.\n";
          } else {
            for (const auto &alert : overuseAlerts)
              report << "- " << alert << "\n";
          }

          if (PDFExporter::exportToPDF(savePath, report.str())) {
            std::cout << "[SUCCESS] PDF effectively saved to: " << savePath
                      << std::endl;
          } else {
            std::cout
                << "[!] Error: Failed to write PDF file. Check permissions."
                << std::endl;
          }
        }
      }

    } else if (choiceStr == "4") {
      std::string resumeText, jobDescText;
      if (!loadAndCheckFiles(resumeFile, jdFile, resumeText, jobDescText)) {
        continue;
      }
      // Extract keywords
      auto jobKeywords = KeywordExtractor::extract(jobDescText, companyIgnoreWords);
      std::vector<std::pair<std::string, int>> missingKeywords;
      std::string lowerResume = toLowerString(resumeText);
      // Match keywords
      for (auto const &[keyword, weight] : jobKeywords) {
        if (lowerResume.find(toLowerString(keyword)) == std::string::npos) {
          missingKeywords.push_back({keyword, weight});
        }
      }
      
      // Standardize output sorting by frequency (descending) then alphabetical
      auto cmp = [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
          if (a.second != b.second) return a.second > b.second;
          return a.first < b.first;
      };
      std::sort(missingKeywords.begin(), missingKeywords.end(), cmp);

      // Keep only top 20
      if (missingKeywords.size() > 20) missingKeywords.resize(20);

      // Write to temporary file
      std::string tempFileName = "temp_resume_edit.txt";
      std::ofstream tempOut(tempFileName);
      tempOut << "=================================================\n";
      tempOut << "DO NOT MODIFY THIS HEADER BLOCK\n";
      tempOut << "[-] Missing Keywords to Add (Top 20):\n";
      if (missingKeywords.empty())
        tempOut << "(None missing!)\n";
      for (const auto &kv : missingKeywords)
        tempOut << "- " << kv.first << " (" << kv.second << ")\n";
      tempOut << "=================================================\n\n";
      tempOut << resumeText;
      tempOut.close();

      std::cout << "\nLaunching text editor... Close the editor window when "
                   "finished to continue."
                << std::endl;

      // Launch system text editor blocking
#ifdef _WIN32
      std::system(("notepad.exe " + tempFileName).c_str());
#elif defined(__APPLE__)
      char resolved[4096] = {};
      std::string absPath = (realpath(tempFileName.c_str(), resolved))
                                ? std::string(resolved)
                                : tempFileName;

      // Escape single quotes in path for the shell
      std::string safePath = absPath;
      size_t pos = 0;
      while ((pos = safePath.find("'", pos)) != std::string::npos) {
        safePath.replace(pos, 1, "'\\''");
        pos += 4;
      }

      // Grab just the filename for the osascript grep
      std::string fname = tempFileName;
      size_t slash = fname.rfind('/');
      if (slash != std::string::npos)
        fname = fname.substr(slash + 1);

      std::string cmd =
          "open -a TextEdit '" + safePath +
          "'; "
          "sleep 1; " // small delay to let TextEdit register the document
          "while osascript -e 'tell app \"TextEdit\" to get name of every "
          "document' "
          "      2>/dev/null | grep -qF '" +
          fname +
          "'; "
          "do sleep 1; done";
      std::system(cmd.c_str());
#else
      std::system(("nano " + tempFileName).c_str());
#endif

      // Read edited file and strip header
      std::ifstream tempIn(tempFileName);
      std::string updatedText = "";
      std::string line;
      bool pastHeader = false;
      while (std::getline(tempIn, line)) {
        if (!pastHeader) {
          if (line == "=================================================") {
            // We hit a delimiter. The header has two delimiters.
            // We will set pastHeader to true on the second one.
            static int delimiterCount = 0;
            delimiterCount++;
            if (delimiterCount == 2) {
              pastHeader = true;
              delimiterCount = 0; // Reset for next time
            }
          }
        } else {
          updatedText += line + "\n";
        }
      }
      tempIn.close();
      std::remove(tempFileName.c_str()); // Clean up temp file

      // Trim leading newlines from updated text
      while (!updatedText.empty() &&
             (updatedText.front() == '\n' || updatedText.front() == '\r')) {
        updatedText.erase(0, 1);
      }

      std::cout << "\nModifications captured! Please select where to save your "
                   "new PDF."
                << std::endl;
      std::string savePath =
          getSaveFilePath("Save PDF As", "PDF Files\0*.pdf\0All Files\0*.*\0");

      if (!savePath.empty()) {
        // Ensure extension is .pdf
        if (savePath.length() < 4 ||
            toLowerString(savePath.substr(savePath.length() - 4)) != ".pdf") {
          savePath += ".pdf";
        }

        if (PDFExporter::exportToPDF(savePath, updatedText)) {
          std::cout << "[SUCCESS] PDF effectively saved to: " << savePath
                    << std::endl;
        } else {
          std::cout << "[!] Error: Failed to write PDF file. Check permissions."
                    << std::endl;
        }
      } else {
        std::cout << "Export cancelled." << std::endl;
      }

    } else if (choiceStr == "5") {
      if (resumeFile.empty() || jdFile.empty()) {
        std::cout << "\n[!] Error: Please select both a Resume and a Job "
                     "Description file first."
                  << std::endl;
        continue;
      }

      if (!ResumeAI::ensureServerRunning()) {
        continue;
      }
      // Read files
      std::string resumeText = FileHandler::readFile(resumeFile);
      std::string jobDescText = FileHandler::readFile(jdFile);
      if (resumeText.empty() || jobDescText.empty()) {
        std::cout << "[!] Error: Could not read files." << std::endl;
        continue;
      }
      // Extract keywords
      auto jobKeywords = KeywordExtractor::extract(jobDescText, companyIgnoreWords);
      std::vector<std::string> matchedKeywords;
      std::vector<std::string> missingKeywords;
      std::string lowerResume = toLowerString(resumeText);
      // Match keywords
      for (auto const &[keyword, weight] : jobKeywords) {
        if (lowerResume.find(toLowerString(keyword)) != std::string::npos) {
          matchedKeywords.push_back(keyword);
        } else {
          missingKeywords.push_back(keyword);
        }
      }
      std::sort(matchedKeywords.begin(), matchedKeywords.end());
      std::sort(missingKeywords.begin(), missingKeywords.end());

      std::cout << "\nCalling AI (default: local Ollama). This may take a "
                   "minute..."
                << std::endl;
      // Suggest improvements
      ResumeAIResult ai = ResumeAI::suggestImprovements(
          resumeText, jobKeywords, matchedKeywords, missingKeywords,
          jobDescText);
      // Check if AI request failed
      if (!ai.ok) {
        std::cout << "\n[!] AI request failed: " << ai.error << std::endl;
        {
          const char *om = std::getenv("OLLAMA_MODEL");
          std::cout << "\nTip: Start by running `ollama pull "
                    << (om && om[0] ? om : "llama3.2")
                    << "` (or set OLLAMA_MODEL). Or set GROQ_API_KEY for Groq."
                    << std::endl;
        }
        continue;
      }
      // Print AI suggestions
      std::cout << "\n--- AI suggestions (" << ai.provider << " / " << ai.model
                << ") ---\n"
                << std::endl;
      std::cout << ai.text << std::endl;
      std::cout << "\n-------------------------------------" << std::endl;

    } else if (choiceStr == "6") {
      std::cout << "Exiting Resume Lens. Goodbye!" << std::endl;
      break;
    } else {
      std::cout << "Invalid option. Please enter 1, 2, 3, 4, 5, or 6."
                << std::endl;
    }
  }

  return 0;
}
