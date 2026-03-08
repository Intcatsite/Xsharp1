#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
    #define CLEANUP "del temp_output.cpp temp_exe.exe"
    #define COMPILE "g++ temp_output.cpp -o temp_exe.exe && temp_exe.exe"
#else
    #define CLEANUP "rm -f temp_output.cpp temp_exe"
    #define COMPILE "g++ temp_output.cpp -o temp_exe && ./temp_exe"
#endif

using namespace std;

void trim(string &s) {
    if (s.empty()) return;
    s.erase(s.find_last_not_of(" \n\r\t") + 1);
    s.erase(0, s.find_first_not_of(" \n\r\t"));
}

string escapeString(const string &s) {
    string result;
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else result += c;
    }
    return result;
}

void cleanup(ofstream &out) {
    out.close();
    system(CLEANUP);
}

int main(int argc, char* argv[]) {
    if (argc < 3 || string(argv[1]) != "play") {
        cout << "Usage: csharp1 play <filename.xs>\n";
        return 1;
    }

    ifstream in(argv[2]);
    if (!in) {
        cout << "Error: File not found.\n";
        return 1;
    }

    ofstream out("temp_output.cpp");
    out << "#include <iostream>\n#include <string>\n#include <cstdlib>\n#include <ctime>\nusing namespace std;\n\nint main() {\n";

    bool randomAllowed = false;
    int braceDepth = 0;
    string line;

    while (getline(in, line)) {
        trim(line);
        if (line.empty() || line.find("type=comments") != string::npos) continue;

        if (line.find("give.me(type_give=module(random))") != string::npos) {
            randomAllowed = true;
            out << "    srand(time(0));\n";
            continue;
        }

        if (line.find("end.block") == 0) {
            if (braceDepth > 0) {
                braceDepth--;
                out << "    }\n";
            }
            continue;
        }

        if (line.find("var(") == 0) {
            if (line.find("module.random") != string::npos) {
                if (!randomAllowed) {
                    cout << "Error: Missing module request for random.\n";
                    in.close();
                    cleanup(out);
                    return 1;
                }
                size_t nameStart = line.find("name=\"") + 6;
                size_t nameEnd = line.find('"', nameStart);
                string vName = line.substr(nameStart, nameEnd - nameStart);

                size_t minStart = line.find("int(") + 4;
                size_t minEnd = line.find(")", minStart);
                string minVal = line.substr(minStart, minEnd - minStart);

                size_t maxStart = line.find("_to_int(") + 8;
                size_t maxEnd = line.find(")", maxStart);
                string maxVal = line.substr(maxStart, maxEnd - maxStart);

                out << "    int " << vName << " = rand() % (" << maxVal << " - " << minVal << " + 1) + " << minVal << ";\n";
            }
            else if (line.find("type_var=input") != string::npos) {
                size_t nameStart = line.find("name=\"") + 6;
                size_t nameEnd = line.find('"', nameStart);
                string vName = line.substr(nameStart, nameEnd - nameStart);

                size_t promptStart = line.find("input(\"") + 7;
                size_t promptEnd = line.find("\")", promptStart);
                string prompt = line.substr(promptStart, promptEnd - promptStart);

                out << "    string " << vName << "_raw;\n";
                out << "    cout << \"" << escapeString(prompt) << " \";\n";
                out << "    getline(cin, " << vName << "_raw);\n";
                out << "    int " << vName << " = atoi(" << vName << "_raw.c_str());\n";
            }
            else {
                size_t nameStart = line.find("name=\"") + 6;
                size_t nameEnd = line.find('"', nameStart);
                string vName = line.substr(nameStart, nameEnd - nameStart);

                size_t valStart = line.find('(', nameEnd) + 1;
                size_t valEnd = line.find(')', valStart);
                string vVal = line.substr(valStart, valEnd - valStart);
                out << "    int " << vName << " = " << vVal << ";\n";
            }
        }
        else if (line.find("?(if_type") == 0) {
            size_t checkStart = line.find("check(\"") + 7;
            size_t checkMid = line.find("\"=\"", checkStart);
            size_t checkEnd = line.find("\")", checkMid);
            string vName = line.substr(checkStart, checkMid - checkStart);
            string vVal = line.substr(checkMid + 3, checkEnd - checkMid - 3);
            out << "    if (" << vName << " == " << vVal << ") {\n";
            braceDepth++;
        }
        else if (line.find("???:") == 0) {
            if (braceDepth > 0) {
                out << "    } else {\n";
            }
        }
        else if (line.find("write(write_type=common.text(\"") != string::npos) {
            size_t start = line.find("text(\"") + 6;
            size_t end = line.rfind("\"))");
            string content = line.substr(start, end - start);
            out << "    cout << \"" << escapeString(content) << "\" << endl;\n";
        }
    }

    while (braceDepth > 0) {
        out << "    }\n";
        braceDepth--;
    }

    out << "    return 0;\n}\n";
    in.close();
    out.close();

    int result = system(COMPILE);
    system(CLEANUP);

    if (result != 0) {
        return 1;
    }

    return 0;
}
