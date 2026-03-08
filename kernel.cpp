#include <iostream>
#include <fstream>
#include <string>
#include <vector>
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

struct Function {
    string name;
    vector<string> body;
};

void parseCondition(const string &line, string &vName, string &op, string &vVal) {
    size_t cs = line.find("check(\"") + 7;
    struct { const char* tok; const char* oper; int len; } ops[] = {
        {"\"!=\"", "!=", 4}, {"\">=\"", ">=", 4}, {"\"<=\"", "<=", 4},
        {"\">\"", ">", 3}, {"\"<\"", "<", 3}, {"\"=\"", "==", 3}
    };
    for (auto &o : ops) {
        size_t p = line.find(o.tok, cs);
        if (p != string::npos) {
            op = o.oper;
            vName = line.substr(cs, p - cs);
            size_t vs = p + o.len;
            size_t ve = line.find("\")", vs);
            vVal = line.substr(vs, ve - vs);
            return;
        }
    }
    op = "=="; vName = ""; vVal = "0";
}

int main(int argc, char* argv[]) {
    if (argc < 3 || string(argv[1]) != "play") {
        cout << "Usage: xsharp play <filename.xs>\n";
        return 1;
    }

    ifstream in(argv[2]);
    if (!in) {
        cout << "Error: File not found.\n";
        return 1;
    }

    vector<string> lines;
    string tmp;
    while (getline(in, tmp)) {
        trim(tmp);
        lines.push_back(tmp);
    }
    in.close();

    vector<Function> functions;
    vector<string> mainCode;
    bool inFunc = false;
    string curFuncName;
    vector<string> curFuncBody;

    for (size_t i = 0; i < lines.size(); i++) {
        string &line = lines[i];
        if (line.empty() || line.find("type=comments") != string::npos || line.find("!") == 0) continue; // Добавил ! как коммент из старого синтаксиса
        if (line.find("func.create(") == 0) {
            inFunc = true;
            size_t ns = line.find("name=\"") + 6;
            curFuncName = line.substr(ns, line.find('"', ns) - ns);
            curFuncBody.clear();
            continue;
        }
        if (line.find("end.func") == 0) {
            Function f; f.name = curFuncName; f.body = curFuncBody;
            functions.push_back(f);
            inFunc = false;
            continue;
        }
        if (inFunc) curFuncBody.push_back(line);
        else mainCode.push_back(line);
    }

    ofstream out("temp_output.cpp");
    out << "#include <iostream>\n#include <string>\n#include <cstdlib>\n#include <ctime>\nusing namespace std;\n\n";

    int depth = 0;

    auto emitLine = [&](const string &line) {
        if (line.find("end.block") == 0 || line.find("end.loop") == 0 || line.find("end.repeat") == 0) {
            if (depth > 0) depth--;
            out << string((depth + 1) * 4, ' ') << "}\n";
            return;
        }
        if (line.find("???:") == 0 || line.find("else:") == 0) {
            out << string(depth * 4, ' ') << "} else {\n";
            return;
        }

        string pad((depth + 1) * 4, ' ');

        // --- БЛОК СТАРОГО СИНТАКСИСА (пасхалка для ленивых) ---
        if (line.find("print(text=\"") == 0) {
            size_t s = line.find("text=\"") + 6;
            size_t e = line.rfind("\")");
            out << pad << "cout << \"" << escapeString(line.substr(s, e - s)) << "\" << endl;\n";
            return;
        }
        else if (line.find("print(var=\"") == 0) {
            size_t s = line.find("var=\"") + 5;
            size_t e = line.rfind("\")");
            out << pad << "cout << " << line.substr(s, e - s) << " << endl;\n";
            return;
        }
        else if (line.find("repeat(") == 0) {
            size_t s = line.find("(") + 1;
            size_t e = line.find("):");
            string count = line.substr(s, e - s);
            out << pad << "for (int i_rep = 0; i_rep < " << count << "; i_rep++) {\n";
            depth++;
            return;
        }
        // --------------------------------------------------------

        if (line.find("give.me(type_give=module(random))") != string::npos) {
            out << pad << "srand(time(0));\n";
        }
        else if (line.find("var(") == 0) {
            size_t ns = line.find("name=\"") + 6;
            size_t ne = line.find('"', ns);
            string vn = line.substr(ns, ne - ns);

            if (line.find("module.random") != string::npos) {
                size_t ms = line.find("int(") + 4;
                size_t me = line.find(")", ms);
                string mi = line.substr(ms, me - ms);
                size_t xs = line.find("_to_int(") + 8;
                size_t xe = line.find(")", xs);
                string mx = line.substr(xs, xe - xs);
                out << pad << "int " << vn << " = rand() % (" << mx << " - " << mi << " + 1) + " << mi << ";\n";
            }
            else if (line.find("type_var=string_input") != string::npos) {
                size_t ps = line.find("string_input(\"") + 14;
                size_t pe = line.find("\")", ps);
                string pr = line.substr(ps, pe - ps);
                out << pad << "string " << vn << ";\n";
                out << pad << "cout << \"" << escapeString(pr) << " \";\n";
                out << pad << "getline(cin, " << vn << ");\n";
            }
            else if (line.find("type_var=input") != string::npos) {
                size_t ps = line.find("input(\"") + 7;
                size_t pe = line.find("\")", ps);
                string pr = line.substr(ps, pe - ps);
                out << pad << "string " << vn << "_raw;\n";
                out << pad << "cout << \"" << escapeString(pr) << " \";\n";
                out << pad << "getline(cin, " << vn << "_raw);\n";
                out << pad << "int " << vn << " = atoi(" << vn << "_raw.c_str());\n";
            }
            else if (line.find("type_var=string(") != string::npos) {
                size_t vs = line.find("string(\"") + 8;
                size_t ve = line.find("\")", vs);
                out << pad << "string " << vn << " = \"" << escapeString(line.substr(vs, ve - vs)) << "\";\n";
            }
            else if (line.find("type_var=math") != string::npos) {
                size_t ms = line.find("math(") + 5;
                size_t me = line.find(")", ms);
                out << pad << "int " << vn << " = " << line.substr(ms, me - ms) << ";\n";
            }
            else {
                size_t vs = line.find('(', ne) + 1;
                size_t ve = line.find(')', vs);
                out << pad << "int " << vn << " = " << line.substr(vs, ve - vs) << ";\n";
            }
        }
        else if (line.find("set(") == 0) {
            size_t ns = line.find("name=\"") + 6;
            size_t ne = line.find('"', ns);
            string vn = line.substr(ns, ne - ns);

            if (line.find("type_var=math") != string::npos) {
                size_t ms = line.find("math(") + 5;
                size_t me = line.find(")", ms);
                out << pad << vn << " = " << line.substr(ms, me - ms) << ";\n";
            }
            else if (line.find("type_var=input") != string::npos) {
                size_t ps = line.find("input(\"") + 7;
                size_t pe = line.find("\")", ps);
                string pr = line.substr(ps, pe - ps);
                out << pad << "cout << \"" << escapeString(pr) << " \";\n";
                out << pad << "getline(cin, " << vn << "_raw);\n";
                out << pad << vn << " = atoi(" << vn << "_raw.c_str());\n";
            }
            else {
                size_t vs = line.find('(', ne) + 1;
                size_t ve = line.find(')', vs);
                out << pad << vn << " = " << line.substr(vs, ve - vs) << ";\n";
            }
        }
        else if (line.find("?(if_type") == 0) {
            string vn, op, vv;
            parseCondition(line, vn, op, vv);
            out << pad << "if (" << vn << " " << op << " " << vv << ") {\n";
            depth++;
        }
        else if (line.find("loop.while(") == 0) {
            string vn, op, vv;
            parseCondition(line, vn, op, vv);
            out << pad << "while (" << vn << " " << op << " " << vv << ") {\n";
            depth++;
        }
        else if (line.find("func.call(") == 0) {
            size_t ns = line.find("name=\"") + 6;
            out << pad << line.substr(ns, line.find('"', ns) - ns) << "();\n";
        }
        // --- ТОТ САМЫЙ ФИКС ДЛЯ КОНКАТЕНАЦИИ ---
        else if (line.find("write(write_type=common.text(") != string::npos) {
            size_t start = line.find("text(") + 5;
            size_t end = line.rfind("))");
            string content = line.substr(start, end - start);
            
            string cpp_out = pad + "cout << ";
            bool in_string = false;
            string current = "";
            
            for (size_t i = 0; i < content.length(); i++) {
                if (content[i] == '"' && !in_string) {
                    in_string = true;
                    cpp_out += current;
                    current = "\"";
                } 
                else if (content[i] == '"' && in_string) {
                    in_string = false;
                    current += "\"";
                    cpp_out += escapeString(current);
                    current = " << ";
                }
                else if (!in_string && isalpha(content[i])) {
                    string var_name = "";
                    while (i < content.length() && (isalnum(content[i]) || content[i] == '_')) {
                        var_name += content[i];
                        i++;
                    }
                    i--;
                    cpp_out += " << " + var_name + " << ";
                }
                else {
                    current += content[i];
                }
            }
            
            if (!current.empty()) {
                if(in_string) cpp_out += escapeString(current);
                else cpp_out += current;
            }
            
            out << cpp_out << " << endl;\n";
        }
        // ----------------------------------------
        else if (line.find("write(write_type=var(") != string::npos) {
            size_t s = line.find("var(") + 4;
            size_t e = line.find("))", s);
            out << pad << "cout << " << line.substr(s, e - s) << " << endl;\n";
        }
        else if (line.find("write(write_type=combine(") != string::npos) {
            size_t pos = line.find("combine(") + 8;
            size_t end = line.rfind("))");
            string inner = line.substr(pos, end - pos);
            out << pad << "cout";
            string token;
            bool inQ = false;
            for (size_t i = 0; i <= inner.size(); i++) {
                char c = (i < inner.size()) ? inner[i] : ',';
                if (c == '"') { inQ = !inQ; continue; }
                if (c == ',' && !inQ) {
                    trim(token);
                    if (!token.empty()) {
                        if (token[0] == '$') out << " << " << token.substr(1);
                        else out << " << \"" << escapeString(token) << "\"";
                    }
                    token.clear();
                    continue;
                }
                token += c;
            }
            trim(token);
            if (!token.empty()) {
                if (token[0] == '$') out << " << " << token.substr(1);
                else out << " << \"" << escapeString(token) << "\"";
            }
            out << " << endl;\n";
        }
    };

    for (size_t i = 0; i < functions.size(); i++) {
        out << "void " << functions[i].name << "() {\n";
        depth = 0;
        for (size_t j = 0; j < functions[i].body.size(); j++)
            emitLine(functions[i].body[j]);
        while (depth > 0) { out << "    }\n"; depth--; }
        out << "}\n\n";
    }

    out << "int main() {\n";
    depth = 0;
    for (size_t i = 0; i < mainCode.size(); i++)
        emitLine(mainCode[i]);
    while (depth > 0) { out << "    }\n"; depth--; }
    out << "    return 0;\n}\n";
    out.close();

    int result = system(COMPILE);
    system(CLEANUP);
    return (result != 0) ? 1 : 0;
}
