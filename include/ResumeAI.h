#ifndef RESUME_AI_H
#define RESUME_AI_H

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file ResumeAI.h
 * @brief Declarations for @ref ResumeAI and @ref ResumeAIResult (Ollama / Groq resume coaching).
 */

/**
 * @struct ResumeAIResult
 * @brief Outcome of a single AI coaching request.
 *
 * On success, @c ok is true, @c text holds the model’s markdown reply, and @c provider / @c model
 * identify which backend ran. On failure, @c ok is false and @c error explains the problem; @c text
 * is usually empty.
 *
 * Default backend is local **Ollama** at `http://127.0.0.1:11434`. If the environment variable
 * `GROQ_API_KEY` is set, the implementation uses Groq’s OpenAI-compatible HTTP API instead (no local
 * server required).
 * @authors Rahul, Anish, Sean, David, Pal
 */
struct ResumeAIResult {
  bool ok = false;       ///< True if the HTTP response was received and assistant content parsed.
  std::string provider;  ///< Backend id, e.g. `"ollama"` or `"groq"`.
  std::string model;     ///< Model tag used for the request (from env or default).
  std::string text;      ///< Assistant message body (markdown sections when the model follows the prompt).
  std::string error;     ///< Human-readable failure reason when @c ok is false.
};

/**
 * @class ResumeAI
 * @brief Sends resume and keyword context to a chat model and optionally ensures Ollama is running.
 *
 * Builds a structured user prompt from the resume, @ref KeywordExtractor frequencies, matched and
 * missing keyword lists, and an optional job-description excerpt. Requests are issued via `curl` to
 * Ollama’s `/api/chat` or Groq’s `/v1/chat/completions`; JSON responses are parsed without extra
 * third-party libraries.
 *
 * Environment variables (optional): `OLLAMA_HOST`, `OLLAMA_MODEL`, `GROQ_API_KEY`, `GROQ_MODEL`.
 * @authors Rahul, Anish, Sean, David, Pal
 */
class ResumeAI {
public:
  /**
   * @brief Asks the configured model to analyze the resume against job keywords and suggest concrete
   * improvements (gap analysis, rewrites, natural placement of missing terms).
   *
   * @authors Rahul, Anish, Sean, David, Pal
   * @param resumeText Full resume text (may be truncated internally for context limits).
   * @param jobKeywords Frequencies from @ref KeywordExtractor on the job description.
   * @param matchedKeywords Keywords from the JD that already appear in the resume.
   * @param missingKeywords JD keywords not found literally in the resume.
   * @param jobDescriptionSnippet Optional JD text for richer context (truncated internally).
   * @return A populated @ref ResumeAIResult; @c ok is false on network, HTTP, or parse errors with
   *         @c error set.
   */
  static ResumeAIResult suggestImprovements(
      const std::string &resumeText,
      const std::unordered_map<std::string, int> &jobKeywords,
      const std::vector<std::string> &matchedKeywords,
      const std::vector<std::string> &missingKeywords,
      const std::string &jobDescriptionSnippet);

  /**
   * @authors Rahul, Anish, Sean, David, Pal
   * @brief Verifies that the Ollama HTTP API is reachable when using the local backend.
   *
   * If `GROQ_API_KEY` is set, returns true immediately (Groq does not need a local server). Otherwise
   * probes `OLLAMA_HOST` (default `127.0.0.1:11434`). If the server is down, attempts to start
   * `ollama serve` in the background (platform-specific) and waits until the port responds or times out.
   *
   * @return True if Ollama is reachable or Groq is configured; false if startup or health check failed.
   */
  static bool ensureServerRunning();
};

#endif // RESUME_AI_H
