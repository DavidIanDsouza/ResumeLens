#include "ResumeAI.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/**
 * @file ResumeAI.cpp
 * @brief Implementation of @ref ResumeAI — chat JSON over HTTP (`curl`), prompt assembly, and optional Ollama startup.
 */

namespace {

/**
 * @brief Returns a copy of @p s with leading/trailing whitespace and line endings removed.
 * 
 * @authors Rahul, Sean, Anish, David, Pal
 * @param s The string to trim.
 * @returns std::string The trimmed string.
 */
std::string trimCopy(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
    s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  return s.substr(i);
}

/**
 * @brief Reads environment variable @p name, or @p fallback if unset or empty.
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param name The name of the environment variable to read.
 * @param fallback The value to return if the environment variable is not set or empty.
 * @return std::string The value of the environment variable, or the fallback if not set.
 */
std::string getenvOr(const char *name, const char *fallback) {
  const char *v = std::getenv(name);
  return (v && v[0]) ? std::string(v) : std::string(fallback);
}

/**
 * @brief Escapes @p s for embedding in a JSON string value (quotes, backslashes, control chars).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param s The string to escape.
 * @return Escaped UTF-8 safe for JSON; non-ASCII bytes pass through unchanged.
 */
std::string jsonEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += static_cast<char>(c);
      }
      break;
    }
  }
  return out;
}

/**
 * @brief Writes @p data to @p path (binary mode, overwrite).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param path The file path to write to.
 * @param data The data to write.
 * @return True on success.
 */
bool writeFile(const std::string &path, const std::string &data) {
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
  f.write(data.data(), static_cast<std::streamsize>(data.size()));
  return static_cast<bool>(f);
}

/**
 * @brief Reads entire file @p path into a string (binary mode).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param path The file path to read from.
 * @return std::string File contents, or empty string if open fails.
 */
std::string readFileAll(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

/**
 * @brief Parses a JSON string value starting at the opening `"` at @p startQuote.
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param j The JSON string to parse from.
 * @param startQuote The index of the opening `"` of the JSON string value.
 * @param[out] out Decoded UTF-8 (non-BMP \\u escapes are approximated).
 * @param[out] endPos Index one past the closing `"` on success.
 * @return True if a well-formed string was read.
 */
bool parseJsonString(const std::string &j, size_t startQuote, std::string &out,
                     size_t &endPos) {
  if (startQuote >= j.size() || j[startQuote] != '"')
    return false;
  size_t i = startQuote + 1;
  std::string acc;
  while (i < j.size()) {
    char c = j[i++];
    if (c == '"') {
      endPos = i;
      out = std::move(acc);
      return true;
    }
    if (c == '\\' && i < j.size()) {
      char e = j[i++];
      switch (e) {
      case '"':
        acc += '"';
        break;
      case '\\':
        acc += '\\';
        break;
      case '/':
        acc += '/';
        break;
      case 'b':
        acc += '\b';
        break;
      case 'f':
        acc += '\f';
        break;
      case 'n':
        acc += '\n';
        break;
      case 'r':
        acc += '\r';
        break;
      case 't':
        acc += '\t';
        break;
      case 'u':
        if (i + 4 <= j.size()) {
          unsigned int cp = 0;
          for (int k = 0; k < 4; ++k) {
            char h = j[i + k];
            int v = 0;
            if (h >= '0' && h <= '9')
              v = h - '0';
            else if (h >= 'a' && h <= 'f')
              v = 10 + (h - 'a');
            else if (h >= 'A' && h <= 'F')
              v = 10 + (h - 'A');
            else {
              acc += "?";
              i += 4;
              goto next_iter;
            }
            cp = (cp << 4) | static_cast<unsigned int>(v);
          }
          i += 4;
          if (cp < 0x80)
            acc += static_cast<char>(cp);
          else
            acc += '?';
        }
        break;
      default:
        acc += e;
        break;
      }
      continue;
    }
    acc += c;
  next_iter:;
  }
  return false;
}

/**
 * @brief Extracts the first JSON string value of a `"content"` key (Ollama / OpenAI-style chat responses).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param json The JSON string to parse.
 * @param[out] content Assistant message body.
 * @return True if a content string was found and parsed.
 */
bool extractMessageContent(const std::string &json, std::string &content) {
  const std::string key = "\"content\"";
  size_t pos = 0;
  while (true) {
    pos = json.find(key, pos);
    if (pos == std::string::npos)
      return false;
    size_t i = pos + key.size();
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
      ++i;
    if (i < json.size() && json[i] == ':') {
      ++i;
      while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
        ++i;
      if (i < json.size() && json[i] == '"') {
        size_t end = 0;
        if (parseJsonString(json, i, content, end))
          return true;
      }
    }
    pos += key.size();
  }
}

/**
 * @brief Parses a top-level JSON `"error"` string field (e.g. Ollama error responses).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param json The JSON string to parse.
 * @param[out] err Error message string.
 * @return True if an error string was found.
 */
bool extractTopLevelError(const std::string &json, std::string &err) {
  const std::string key = "\"error\"";
  size_t pos = json.find(key);
  if (pos == std::string::npos)
    return false;
  size_t i = pos + key.size();
  while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
    ++i;
  if (i >= json.size() || json[i] != ':')
    return false;
  ++i;
  while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
    ++i;
  if (i < json.size() && json[i] == '"') {
    size_t end = 0;
    return parseJsonString(json, i, err, end);
  }
  return false;
}

/**
 * @brief Truncates @p s to @p maxChars with a trailing note if shortened.
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param s The string to potentially truncate.
 * @param maxChars The maximum number of characters to keep before truncation.
 * @return std::string The original string if within limit, or a truncated version with a note if it exceeds the limit.
 */
std::string truncateWithNote(const std::string &s, size_t maxChars) {
  if (s.size() <= maxChars)
    return s;
  return s.substr(0, maxChars) +
         "\n\n[Truncated for model context — edit locally if needed.]\n";
}

/**
 * @brief Formats job keyword frequencies as sorted bullet lines (top 48 by weight).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param jobKeywords Map of keyword to frequency weight.
 * @return std::string Formatted bullet list of keywords and their weights.
 */
std::string formatKeywordLines(
    const std::unordered_map<std::string, int> &jobKeywords) {
  std::vector<std::pair<std::string, int>> pairs(jobKeywords.begin(),
                                                   jobKeywords.end());
  std::sort(pairs.begin(), pairs.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  std::ostringstream oss;
  size_t n = 0;
  for (const auto &p : pairs) {
    oss << "- " << p.first << " (weight " << p.second << ")\n";
    if (++n >= 48)
      break;
  }
  return oss.str();
}

/**
 * @brief Joins keyword names as `- item` lines for the prompt.
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param items The list of keyword strings to format.
 * @return std::string Formatted bullet list of keywords.
 */
std::string joinLines(const std::vector<std::string> &items) {
  std::ostringstream oss;
  for (const auto &s : items)
    oss << "- " << s << "\n";
  return oss.str();
}

/**
 * @brief Assembles the user message sent to the chat model (instructions + resume + keyword blocks + optional JD).
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param resumeText The text of the resume being analyzed.
 * @param kwBlock The formatted block of job keywords and their weights.
 * @param matchedBlock The formatted block of keywords already present in the resume.
 * @param missingBlock The formatted block of keywords missing from the resume.
 * @param jdSnippet An optional excerpt from the job description for additional context.
 * @return std::string The complete user message to send to the model.
 */
std::string buildUserPrompt(const std::string &resumeText,
                            const std::string &kwBlock,
                            const std::string &matchedBlock,
                            const std::string &missingBlock,
                            const std::string &jdSnippet) {
  std::ostringstream oss;
  oss << "You are an expert resume coach and ATS-aware editor.\n\n"
         "Use the job-description keyword frequencies as importance weights. "
         "Do not invent employers, degrees, dates, or skills the candidate "
         "does not plausibly have; suggest honest rewrites and where to add "
         "missing terms naturally.\n\n"
         "=== RESUME TEXT ===\n"
      << resumeText << "\n\n"
      << "=== JOB KEYWORD FREQUENCIES (from JD) ===\n"
      << kwBlock << "\n"
      << "=== MATCHED KEYWORDS (already in resume) ===\n"
      << matchedBlock << "\n"
      << "=== MISSING KEYWORDS (not found literally in resume) ===\n"
      << missingBlock << "\n";
  if (!jdSnippet.empty()) {
    oss << "\n=== JOB DESCRIPTION EXCERPT (context) ===\n" << jdSnippet << "\n";
  }
  oss << "\nRespond with clear markdown sections:\n"
         "## Gap analysis\n"
         "## Suggested edits\n"
         "  - Quote a short original snippet, then give a revised version.\n"
         "## Keywords to weave in (without stuffing)\n"
         "## Optional summary bullet for profile/summary (if present)\n\n"
         "IMPORTANT: Strictly provide ONLY the markdown output. Do NOT include any conversational filler, greetings, or conclusions (e.g., 'Please let me know if you need any further assistance!').\n";
  return oss.str();
}

/**
 * @brief Runs @p cmd (a full `curl` command) and writes HTTP body to @p outPath.
 * 
 * @authors Rahul, Anish, Sean, David, Pal
 * @param cmd The full command line to execute (should include all headers and data).
 * @param outPath The path to the file where the HTTP body will be written.
 * @return Process exit code (0 usually means HTTP success when curl completes).
 */
int runCurlToFile(const std::string &cmd, const std::string &outPath) {
#ifdef _WIN32
  std::string full = cmd + " -o \"" + outPath + "\"";
  
  std::vector<char> cmdBuf(full.begin(), full.end());
  cmdBuf.push_back('\0');

  STARTUPINFOA si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));

  HANDLE hJob = CreateJobObjectA(NULL, NULL);
  if (hJob) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
  }

  if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
    if (hJob) {
      AssignProcessToJobObject(hJob, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (hJob) CloseHandle(hJob);
    return static_cast<int>(exitCode);
  } else {
    if (hJob) CloseHandle(hJob);
    return std::system(full.c_str());
  }
#else
  std::string full = cmd + " -o '" + outPath + "'";
  int rc = std::system(full.c_str());
  return rc;
#endif
}

} // namespace

/*!
 * \copydoc ResumeAI::suggestImprovements
 */
ResumeAIResult ResumeAI::suggestImprovements(
    const std::string &resumeText,
    const std::unordered_map<std::string, int> &jobKeywords,
    const std::vector<std::string> &matchedKeywords,
    const std::vector<std::string> &missingKeywords,
    const std::string &jobDescriptionSnippet) {

  ResumeAIResult r;
  const std::string groqKey = getenvOr("GROQ_API_KEY", "");
  const bool useGroq = !groqKey.empty();

  if (useGroq) {
    r.provider = "groq";
    r.model = getenvOr("GROQ_MODEL", "llama-3.3-70b-versatile");
  } else {
    r.provider = "ollama";
    r.model = getenvOr("OLLAMA_MODEL", "llama3.2");
  }

  const std::string resume =
      truncateWithNote(resumeText, 14000);
  const std::string jd =
      jobDescriptionSnippet.empty()
          ? std::string()
          : truncateWithNote(jobDescriptionSnippet, 8000);

  const std::string kwBlock = formatKeywordLines(jobKeywords);
  const std::string matchedBlock =
      matchedKeywords.empty() ? "(none)\n" : joinLines(matchedKeywords);
  const std::string missingBlock =
      missingKeywords.empty() ? "(none)\n" : joinLines(missingKeywords);

  const std::string userContent =
      buildUserPrompt(resume, kwBlock, matchedBlock, missingBlock, jd);
  const std::string userEsc = jsonEscape(userContent);

  const std::string reqPath = "temp_ai_request.json";
  const std::string respPath = "temp_ai_response.json";

  std::string jsonBody;
  if (useGroq) {
    jsonBody = "{\"model\":\"" + r.model +
               "\",\"temperature\":0.4,\"messages\":[{\"role\":\"user\",\"content\":\"" +
               userEsc + "\"}]}";
  } else {
    jsonBody = "{\"model\":\"" + r.model +
               "\",\"stream\":false,\"messages\":[{\"role\":\"user\",\"content\":\"" +
               userEsc + "\"}]}";
  }

  if (!writeFile(reqPath, jsonBody)) {
    r.error = "Could not write temp_ai_request.json";
    return r;
  }

#ifdef _WIN32
  std::string curlBase = "curl.exe -s -S -X POST ";
#else
  std::string curlBase = "curl -s -S -X POST ";
#endif
  std::string cmd;
  if (useGroq) {
    cmd = curlBase + "-H \"Content-Type: application/json\" -H \"Authorization: Bearer " +
          groqKey + "\" ";
    cmd += "--data-binary @\"" + reqPath + "\" ";
    cmd += "\"https://api.groq.com/openai/v1/chat/completions\"";
  } else {
    const std::string host = getenvOr("OLLAMA_HOST", "127.0.0.1:11434");
    cmd = curlBase + "-H \"Content-Type: application/json\" ";
    cmd += "--data-binary @\"" + reqPath + "\" ";
    cmd += "\"http://" + host + "/api/chat\"";
  }

  std::remove(respPath.c_str());
  int rc = runCurlToFile(cmd, respPath);
  std::string response = readFileAll(respPath);
  ::remove(reqPath.c_str());
  ::remove(respPath.c_str());

  if (rc != 0) {
    std::string errMsg;
    if (extractTopLevelError(response, errMsg)) {
      r.error = "HTTP/curl error (code " + std::to_string(rc) + "): " + errMsg;
    } else if (!response.empty()) {
      r.error = "Request failed (curl exit " + std::to_string(rc) +
                "). Response: " + truncateWithNote(response, 800);
    } else {
      r.error =
          "Request failed (curl exit " + std::to_string(rc) +
          "). Is `curl` installed and reachable? For Ollama, is the server "
          "running (ollama serve) and is the model pulled (`ollama pull " +
          r.model + "`)?";
    }
    return r;
  }

  std::string content;
  if (!extractMessageContent(response, content)) {
    if (extractTopLevelError(response, content)) {
      r.error = content;
      return r;
    }
    r.error =
        "Could not parse model response. Raw (truncated): " +
        truncateWithNote(trimCopy(response), 1200);
    return r;
  }

  r.ok = true;
  r.text = std::move(content);
  return r;
}

#ifdef _WIN32
/** Job object and process handle so the Ollama child exits when Resume Lens exits (Windows). */
static HANDLE g_hOllamaJob = NULL;
static HANDLE g_hOllamaProcess = NULL;
#endif

/*!
 * \copydoc ResumeAI::ensureServerRunning
 */
bool ResumeAI::ensureServerRunning() {
  const std::string groqKey = getenvOr("GROQ_API_KEY", "");
  if (!groqKey.empty()) {
    return true; // Hosted API, no local server needed
  }

  const std::string host = getenvOr("OLLAMA_HOST", "127.0.0.1:11434");

#ifdef _WIN32
  // We use -f to fail silently on server errors, -m 2 for a fast 2-second timeout
  std::string checkCmd = "curl.exe -f -s -S -m 2 \"http://" + host + "/\" > NUL 2>&1";
#else
  std::string checkCmd = "curl -f -s -S -m 2 \"http://" + host + "/\" > /dev/null 2>&1";
#endif

  auto isRunning = [&]() {
    return std::system(checkCmd.c_str()) == 0;
  };

  if (isRunning()) {
    return true; // Already running, proceed like normal
  }

  std::cout << "\n[i] Ollama server is not running. Starting it in the background... " << std::flush;

#ifdef _WIN32
  std::string launchCmd = "ollama.exe serve";
  std::vector<char> cmdBuf(launchCmd.begin(), launchCmd.end());
  cmdBuf.push_back('\0');

  STARTUPINFOA si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE; // Run completely invisibly in the background

  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));

  if (!g_hOllamaJob) {
    g_hOllamaJob = CreateJobObjectA(NULL, NULL);
    if (g_hOllamaJob) {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
      // Ensures the server stops when you close ResumeLens
      jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      SetInformationJobObject(g_hOllamaJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
  }

  if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
    if (g_hOllamaJob) {
      AssignProcessToJobObject(g_hOllamaJob, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    g_hOllamaProcess = pi.hProcess; // Keep handle open so the process lives until app exits
    CloseHandle(pi.hThread);
  } else {
    std::cout << "[Failed!]\n[!] Error: 'ollama' command not found or failed to start. Tip: You may need to download and install Ollama from ollama.com" << std::endl;
    return false;
  }
#else
  std::system("ollama serve >/dev/null 2>&1 &");
#endif

  // Wait for the server to spin up and bind to the port (up to 15 seconds)
  for (int i = 0; i < 15; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (isRunning()) {
      std::cout << "[OK]\n";
      return true;
    }
  }

  std::cout << "[Failed!]\n[!] Error: Tried to start Ollama but it didn't respond in time. Make sure Ollama is installed." << std::endl;
  return false;
}
