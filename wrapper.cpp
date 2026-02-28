#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <set>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ============================================================================
// CONFIGURATION
// ============================================================================
const int DEBUG_LEVEL = 0;      // 0 = Silent, 1 = Verbose 
const std::string BATCH_NAME = "run_docker.bat";
const std::string TEMP_CPP   = "obsidian_code.cpp";
// ============================================================================

// Hilfsfunktion: Sucht eine Datei rekursiv im Projektstamm
fs::path findFileRecursive(const fs::path& root, const std::string& filename) {
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_directory() && entry.path().filename() == filename) {
            return entry.path();
        }
    }
    return "";
}

// Selbsttest-Funktion (Debug-Infos hier sind UNABHÄNGIG von DEBUG_LEVEL)
void runSelfTest(const fs::path& exeDir, const fs::path& batchPath) {
    std::cout << "==========================================================\n";
    std::cout << "[SELF-TEST] STARTING DOCKER-GCC DIAGNOSTICS\n";
    std::cout << "==========================================================\n";

    fs::path testDir = exeDir / "test_env";
    fs::path incDir  = testDir / "include";
    fs::path srcDir  = testDir / "src";
    fs::create_directories(incDir);
    fs::create_directories(srcDir);

    // Test-Dateien erzeugen
    std::ofstream hFile(incDir / "test_logic.h");
    hFile << "#pragma once\nint multiply(int a, int b);";
    hFile.close();

    std::ofstream cFile(srcDir / "test_logic.cpp");
    cFile << "#include \"test_logic.h\"\nint multiply(int a, int b) { return a * b; }";
    cFile.close();

    std::ofstream tFile(exeDir / TEMP_CPP);
    tFile << "#include <iostream>\n#include \"test_logic.h\"\nint main() { std::cout << \"[TEST] 6 * 7 = \" << multiply(6, 7) << std::endl; return 0; }";
    tFile.close();

    std::cout << "  [1] Test files generated in: " << testDir << "\n";

    fs::path localBatch = batchPath;
    std::string cmd = "\"\"" + localBatch.make_preferred().string() + "\" \"-std=c++20\" --includes \"" + fs::absolute(incDir).string() + "\"\"";
    
    std::cout << "  [2] Calling system: " << cmd << "\n\n";
    int res = std::system(cmd.c_str());

    std::cout << "\n----------------------------------------------------------\n";
    if (res == 0) std::cout << "[RESULT] SELF-TEST PASSED!\n";
    else std::cout << "[RESULT] SELF-TEST FAILED!\n";

    // Cleanup
    if (fs::exists(testDir)) fs::remove_all(testDir);
    if (fs::exists(exeDir / TEMP_CPP)) fs::remove(exeDir / TEMP_CPP);
    std::cout << "==========================================================\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    fs::path exeDir       = fs::absolute(argv[0]).parent_path();
    fs::path batchPath    = exeDir / BATCH_NAME;
    fs::path tempFilePath = exeDir / TEMP_CPP;

    // Check for Self-Test Flag
    if (argc > 1) {
        std::string firstArg = argv[1];
        if (firstArg == "-t" || firstArg == "-test") {
            runSelfTest(exeDir, batchPath);
            return 0;
        }
    }

    std::set<std::string> flags;
    std::set<std::string> includePaths;
    std::stringstream finalCode;
    int codeLineCount = 0;

    if (DEBUG_LEVEL >= 1) {
        std::cout << "==========================================================\n";
        std::cout << "[WRAPPER] ANALYZING ARGUMENTS & SOURCE CODE\n";
        std::cout << "==========================================================\n";
    }

    for (int i = 1; i < argc; ++i) {
        std::stringstream argStream(argv[i]);
        std::string line;
        
        while (std::getline(argStream, line)) {
            if (line.empty()) continue;

            // --- INCLUDE LOGIC ---
            if (line.find("#include") != std::string::npos && line.find("\"") != std::string::npos) {
                size_t start = line.find("\"");
                size_t end = line.find_last_of("\"");
                
                if (start != std::string::npos && end != std::string::npos && start < end) {
                    std::string pathStr = line.substr(start + 1, end - start - 1);
                    
                    // Path cleaning from old wrapper
                    pathStr.erase(std::remove(pathStr.begin(), pathStr.end(), '\"'), pathStr.end());
                    std::replace(pathStr.begin(), pathStr.end(), '/', '\\');

                    fs::path p(pathStr);
                    std::string filename = p.filename().string();
                    fs::path foundPath = p;

                    // Neue Intelligenz: Rekursive Suche, falls Pfad nicht exakt stimmt
                    if (!fs::exists(foundPath)) {
                        if (DEBUG_LEVEL >= 1) std::cout << "  [SEARCH] " << filename << " not at exact path. Scanning project...\n";
                        foundPath = findFileRecursive(exeDir, filename);
                    }

                    if (!foundPath.empty() && fs::exists(foundPath)) {
                        includePaths.insert(fs::absolute(foundPath.parent_path()).string());
                        finalCode << "#include \"" << filename << "\"\n";
                        if (DEBUG_LEVEL >= 1) std::cout << "  [INCLUDE] Found: " << foundPath << "\n";
                    } else {
                        finalCode << line << "\n"; // Fallback
                        if (DEBUG_LEVEL >= 1) std::cout << "  [WARN] Include not found: " << filename << "\n";
                    }
                    continue;
                }
            } 
            // --- FLAG LOGIC ---
            else if (line[0] == '-' && line.find("#include") == std::string::npos) {
                if (DEBUG_LEVEL >= 1) std::cout << "  [FLAG]    " << line << "\n";
                flags.insert(line);
            } 
            // --- CODE LOGIC ---
            else {
                finalCode << line << "\n";
                codeLineCount++;
            }
        }
    }

    // Write temp file
    std::ofstream out(tempFilePath);
    out << finalCode.str();
    out.close();

    // Prepare System Call
    fs::path localBatch = batchPath;
    std::string cleanBatchPath = localBatch.make_preferred().string();
    std::stringstream cmdStream;
    cmdStream << "\"\"" << cleanBatchPath << "\" ";
    for (const auto& f : flags) cmdStream << "\"" << f << "\" ";
    cmdStream << "--includes ";
    for (const auto& p : includePaths) cmdStream << "\"" << p << "\" ";
    cmdStream << "\"";

    if (DEBUG_LEVEL >= 1) {
        std::cout << "----------------------------------------------------------\n";
        std::cout << "[WRAPPER] PROCESSING INFO:\n";
        std::cout << "  Lines of code written: " << codeLineCount << "\n";
        std::cout << "  Include directories:    " << includePaths.size() << "\n";
        std::cout << "----------------------------------------------------------\n";
        std::cout << "[WRAPPER] SYSTEM CALL:\n";
        std::cout << "  " << cmdStream.str() << "\n";
        std::cout << "==========================================================\n\n";
    }

    fs::current_path(exeDir);
    int result = std::system(cmdStream.str().c_str());

    // Cleanup nach dem Run
    if (fs::exists(tempFilePath)) fs::remove(tempFilePath);

    return result;
}