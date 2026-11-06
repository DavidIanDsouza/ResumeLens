# Resume Lens 🔍
### Made by: David, Sean, Anish, Rahul and Pal

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)

Resume Lens is a fast, C++ based resume keyword analyzer designed to help candidates optimize their resumes against specific job descriptions. By parsing both PDF files natively, it calculates a match percentage, checks formatting consistency, and alerts users to overused buzzwords.

## Architecture

The project is structured with a modular, object-oriented design intended for team collaboration:

*   **FileHandler**: Extracts raw text natively from `.pdf` files using a bundled `pdftotext.exe` utility, ensuring no external Python or library dependencies are required for end-users.
*   **KeywordExtractor**: Processes job descriptions to extract the core requirements and skills.
*   **FormattingAnalyzer**: Checks for capitalization and punctuation consistency across the resume to ensure a professional presentation.
*   **JobMatch**: Compares the resume content against extracted job keywords to calculate an ATS (Applicant Tracking System) match percentage.
*   **KeywordAlerts**: Scans the resume for unnaturally overused keywords or "keyword stuffing."
*   **PDFExporter**: A custom, lightweight native C++ generator that measures strings, wraps lines, detects headings, and outputs properly formatted PDF-1.4 documents without requiring any heavyweight third-party bindings.
*   **ResumeAI**: Sends resume text plus extracted keyword context to **Ollama** (default) or **Groq** via `curl`, then prints structured coaching (defaults: `OLLAMA_MODEL=llama3.2`, or Groq when `GROQ_API_KEY` is set).

### AI resume coach (Ollama by default)

The app can call a chat model with your **resume text**, **job keyword frequencies**, **matched/missing keyword lists**, and a **job description excerpt** so you get gap analysis and concrete rewrite suggestions.

- **Default: [Ollama](https://ollama.com)** (local, no API quota). Install Ollama, run `ollama serve`, pull a model (e.g. `ollama pull llama3.2`), then use menu **5 — AI Resume Coach**. Optional env vars: `OLLAMA_HOST` (default `127.0.0.1:11434`), `OLLAMA_MODEL` (default `llama3.2`).
- **Optional: Groq** — set `GROQ_API_KEY`; the app switches to Groq’s OpenAI-compatible API. Optional: `GROQ_MODEL` (default `llama-3.3-70b-versatile`).

Requests use `curl` (included on Windows 10+, macOS, and typical Linux). No extra C++ libraries are required.

## Features

*   **Load and Read Resume**: The system can load and read resume text from PDF files.
*   **Keyword Comparison**: The system checks which keywords from the job description are present in the resume.
*   **Job Match Percentage**: The system generates a final match percentage score based on keyword relevance.
*   **Formatting Consistency Analysis**: The system checks the resume for consistent capitalization and punctuation.
*   **Keyword Overuse Alerts**: The system tracks word frequencies to prevent "keyword stuffing." It utilizes a large dictionary of over 150+ natural filler words (e.g., *using, ability, action*) to ensure normal conversation isn't falsely flagged.
*   **Smart Document Detection**: The system evaluates keyword density to automatically warn you if you accidentally select your Job Description in the Resume prompt and vice versa, offering to silently swap them in the background.
*   **Interactive Menu & Native GUI Dialogs**: The application features a persistent CLI menu that bridges into your Operating System's native file explorer (Windows `comdlg32` / macOS `osascript`) so you can select your resumes and job descriptions using visual UI windows instead of typing out messy command-line paths.
*   **AI Resume Coach**: Menu option sends keyword frequencies, matched/missing lists, resume text, and job description context to a local Ollama model (or Groq if configured) for gap analysis and rewrite suggestions.
*   **Manual Edit & Smart PDF Export**: Users can launch their system text editor directly from the app to tweak their resume, seamlessly referencing missing keywords at the top of the file. The app then natively generates a brand-new PDF applying smart typography algorithms (1-inch margins, Helvetica fonts, dynamic bolding for headings).

## Prerequisites
To build the project from source, you will need:
*   A C++17 compatible compiler (e.g., MinGW `g++` on Windows, or `g++` on Linux).

### Windows
*Note: The project bundles `pdftotext.exe` (from the open-source Xpdf project) directly in the root directory to handle PDF parsing natively on Windows machines. No additional installation is required.*

### Linux / SSH Environments
If you are compiling and running the project on a Linux machine (such as a university SSH server), the bundled `.exe` will not work. You must ensure the native Linux `pdftotext` package is installed on the machine:
*   Ubuntu/Debian: `sudo apt install poppler-utils`
*(Note: Most university lab servers already have this utility pre-installed).*

### macOS
MacOS users can run Resume Lens locally, but must first install the `pdftotext` package via Homebrew:
*   `brew install poppler`

## Setting up the AI Resume Coach (Option 5)

Resume Lens features an integrated "AI Resume Coach" that generates personalized gap analysis and resume rewrite suggestions. By default, it operates locally using **Ollama**. If you configure Groq, resume and job description content is sent to Groq instead.

### 1. Getting Started with Ollama

* **Install Ollama**: Download and install it for your operating system from **[ollama.com](https://ollama.com)**.
* **Download the Model**: Open your terminal or command prompt and run:
  ```powershell
  ollama pull llama3.2
  ```
  *(Note: This downloads the default AI model which is ~2.0GB. It may take a few minutes depending on your internet connection).*
* **Use Option 5 in Resume Lens**: You do **not** need to manually start the AI server! When you select **Option 5** from the main menu, Resume Lens will automatically evaluate your files, start the Ollama engine in the background if it isn't already running, and generate your tailored coaching tips.
  * *Please note: Generating responses locally requires compute power. It may take 30-60 seconds to produce the suggestions as the model streams its feedback.*

### 2. Troubleshooting: "ollama is not recognized as an internal or external command"
If you install Ollama but your terminal says the command is not recognized, you likely need to add it to your Windows System Path:
1. Open the Windows Start Menu and search for **"Environment Variables"** -> Click **"Edit the system environment variables"**.
2. Click the **"Environment Variables..."** button at the bottom.
3. Under **User variables**, select `Path` and click **Edit**.
4. Click **New** and add the default Ollama installation path:
   `C:\Users\<YourUsername>\AppData\Local\Programs\Ollama` *(Replace `<YourUsername>` with your actual Windows username)*.
5. Click **OK** on all windows, completely close your IDE/Terminal, and reopen it. `ollama` commands should now work!

### 3. Alternative: Using Groq
If your system cannot run Ollama, you can use Groq instead. Set the
`GROQ_API_KEY` system environment variable with your API key, and the app will
route requests through Groq's API. When Groq is enabled, resume and job
description content is sent to Groq, so review its privacy and usage policies
before using this option.

## Building and Running

The project uses CMake and requires a C++17-compatible compiler.

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run the application from the generated build directory:

```powershell
.\build\Debug\resume_ai.exe
```

On Linux or macOS, the executable is normally located at
`./build/resume_ai` after building:

```bash
./build/resume_ai
```

## Usage Workflow

The application uses an interactive menu, so you do not need to hardcode
paths or use command-line arguments.

1. Launch `resume_ai.exe` on Windows or `./resume_ai` on macOS/Linux.
2. **Select Option 1**: A native File Explorer window will pop up allowing you to visually browse and select your `Resume.pdf`.
3. **Select Option 2**: Visually browse and select the `Job_Description.pdf`.
4. **Select Option 3**: The app will instantly analyze the documents and output your Match Score, Missing Keywords, Overuse Alerts, and Formatting Analysis directly to the console.
5. **(Optional) Select Option 5**: With Ollama running locally (or `GROQ_API_KEY` set), the app sends the same keyword context to the model and prints markdown-style coaching (gap analysis, suggested edits, keywords to weave in).
6. **Select Option 4**: The app will suspend and open your resume directly inside your system's text editor (e.g., Notepad). All of your missing keywords are conveniently injected at the top of the file. Simply edit your text, save, and close Notepad. The application will resume, prompt you for a save destination, and seamlessly export your tailored text into a beautifully formatted `PDF`!
7. **Select Option 6** when you are done to exit.

## Project Structure
```
Resume Lens/
├── .vscode/               # Editor configuration files
├── include/               # Header files (.h)
│   ├── FileHandler.h
│   ├── FormattingAnalyzer.h
│   ├── JobMatch.h
│   ├── KeywordAlerts.h
│   ├── KeywordExtractor.h
│   ├── PDFExporter.h
│   └── ResumeAI.h
├── src/                   # Source files (.cpp)
│   ├── FileHandler.cpp
│   ├── FormattingAnalyzer.cpp
│   ├── JobMatch.cpp
│   ├── KeywordAlerts.cpp
│   ├── KeywordExtractor.cpp
│   ├── main.cpp
│   ├── PDFExporter.cpp
│   └── ResumeAI.cpp
├── .gitignore             # Ignored files for version control
├── CMakeLists.txt         # CMake build configuration
├── Doxyfile               # Doxygen configuration file
├── pdftotext.exe          # Bundled Xpdf utility for native text extraction
├── README.md              # Project overview and instructions
```

Build output, generated documentation, executables, and local PDF test
documents are intentionally excluded from version control. Add anonymized
sample documents separately if reproducible examples are needed.
