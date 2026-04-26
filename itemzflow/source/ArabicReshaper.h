#ifndef ARABIC_RESHAPER_H
#define ARABIC_RESHAPER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

class ArabicReshaper {
private:
    static std::unordered_map<std::string, std::string>& getReverseDict() {
        static std::unordered_map<std::string, std::string> reverse_dict;
        return reverse_dict;
    }

    struct CharUnit {
        std::string base;
        std::string tashkeel;
        char type; 
        std::string shaped;
    };

    struct Segment {
        char type;
        std::vector<CharUnit> units;
    };

    struct Glyph {
        std::string isolated, end, middle, beginning;
    };

public:
    static std::string Reshape(const std::string& utf8_text) {
        if (utf8_text.empty()) return "";

        std::vector<std::string> chars = splitUTF8(utf8_text);
        std::vector<CharUnit> units;

        for (const auto& c : chars) {
            if (isTashkeel(c) && !units.empty()) {
                units.back().tashkeel += c; 
            } else {
                CharUnit u;
                u.base = c;
                u.tashkeel = "";
                u.type = getType(c);
                u.shaped = c;
                units.push_back(u);
            }
        }

        for (size_t i = 0; i < units.size(); i++) {
            if (units[i].type == 'N') {
                char left = 'R'; 
                for (int j = i - 1; j >= 0; j--) {
                    if (units[j].type != 'N') { left = units[j].type; break; }
                }
                char right = 'R'; 
                for (size_t j = i + 1; j < units.size(); j++) {
                    if (units[j].type != 'N') { right = units[j].type; break; }
                }
                units[i].type = (left == right) ? left : 'R';
            }
        }

        for (size_t i = 0; i < units.size(); i++) {
            if (units[i].type == 'R' && isArabic(units[i].base)) {
                if (units[i].base == "ل" && i + 1 < units.size() && isAlef(units[i+1].base)) {
                    bool connect_before = false;
                    for(int j = i - 1; j >= 0; j--) {
                        if (units[j].base != "") { connect_before = canConnect(units[j].base); break; }
                    }
                    units[i].shaped = getLamAlef(units[i+1].base, connect_before);
                    units[i].tashkeel += units[i+1].tashkeel; 
                    
                    units[i].base = "ا"; 
                    
                    units[i+1].base = ""; 
                    units[i+1].tashkeel = "";
                    continue;
                }

                bool connect_before = false;
                for(int j = i - 1; j >= 0; j--) {
                    if (units[j].base != "") { connect_before = canConnect(units[j].base); break; }
                }

                bool connect_after = false;
                for(size_t j = i + 1; j < units.size(); j++) {
                    if (units[j].base != "") { 
                        connect_after = canConnectBefore(units[j].base) && canConnect(units[i].base); 
                        break; 
                    }
                }
                units[i].shaped = getGlyph(units[i].base, connect_before, connect_after);
            }
        }

        std::vector<Segment> segments;
        if (!units.empty()) {
            segments.push_back({units[0].type, {}});
        }
        for (auto& u : units) {
            if (u.base == "" && u.tashkeel == "") continue;
            if (u.type != segments.back().type) {
                segments.push_back({u.type, {}});
            }
            segments.back().units.push_back(u);
        }

        std::string final_result = "";
        for (int i = segments.size() - 1; i >= 0; i--) {
            if (segments[i].type == 'R') {
                for (int j = segments[i].units.size() - 1; j >= 0; j--) {
                    std::string shaped = segments[i].units[j].shaped;
                    shaped = mirrorPunctuation(shaped);
                    final_result += segments[i].units[j].tashkeel + shaped; 
                }
            } else {
                for (size_t j = 0; j < segments[i].units.size(); j++) {
                    final_result += segments[i].units[j].tashkeel + segments[i].units[j].shaped;
                }
            }
        }

        getReverseDict()[final_result] = utf8_text;

        return final_result;
    }

    static std::string RestoreRaw(std::string text) {
        if (text.empty()) return "";
        for (const auto& entry : getReverseDict()) {
            size_t pos = 0;
            while ((pos = text.find(entry.first, pos)) != std::string::npos) {
                text.replace(pos, entry.first.length(), entry.second);
                pos += entry.second.length();
            }
        }
        return text;
    }

private:
    static std::vector<std::string> splitUTF8(const std::string& str) {
        std::vector<std::string> result;
        size_t i = 0;
        while (i < str.length()) {
            size_t len = 1;
            unsigned char c = str[i];
            if ((c & 0xF8) == 0xF0) len = 4;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xE0) == 0xC0) len = 2;
            result.push_back(str.substr(i, len));
            i += len;
        }
        return result;
    }

    static bool isTashkeel(const std::string& c) {
        if (c.length() == 2) {
            unsigned char b1 = c[0], b2 = c[1];
            return (b1 == 0xD9 && b2 >= 0x8B && b2 <= 0x9F);
        }
        return false;
    }

    static bool isArabic(const std::string& c) {
        if (c.length() >= 2) {
            unsigned char b1 = c[0], b2 = c[1];
            if (b1 == 0xD8 || b1 == 0xD9) {
                if (b1 == 0xD9 && b2 >= 0x8B && b2 <= 0x9F) return false; 
                return true;
            }
            if (b1 == 0xEF) return true;
        }
        return false;
    }

    static char getType(const std::string& c) {
        if (isArabic(c)) return 'R';
        if (c.length() == 1) {
            unsigned char ch = c[0];
            if (isalnum(ch)) return 'L';
            return 'N';
        }
        return 'L'; 
    }

    static bool isAlef(const std::string& c) {
        return c == "ا" || c == "أ" || c == "إ" || c == "آ";
    }

    static std::string getLamAlef(const std::string& alef, bool before) {
        if (alef == "ا") return before ? "ﻼ" : "ﻻ";
        if (alef == "أ") return before ? "ﻸ" : "ﻷ";
        if (alef == "إ") return before ? "ﻶ" : "ﻹ";
        if (alef == "آ") return before ? "ﻶ" : "ﻵ";
        return "ﻻ";
    }

    static bool canConnect(const std::string& c) {
        if (c == "ا" || c == "أ" || c == "إ" || c == "آ" || c == "د" || c == "ذ" || 
            c == "ر" || c == "ز" || c == "و" || c == "ؤ" || c == "ة" || c == "ى") return false;
        return isArabic(c);
    }
    
    static bool canConnectBefore(const std::string& c) {
        return isArabic(c) && c != "ء";
    }

    static std::string mirrorPunctuation(const std::string& c) {
        if (c == "(") return ")";
        if (c == ")") return "(";
        if (c == "[") return "]";
        if (c == "]") return "[";
        if (c == "{") return "}";
        if (c == "}") return "{";
        if (c == "<") return ">";
        if (c == ">") return "<";
        if (c == "«") return "»";
        if (c == "»") return "«";
        return c;
    }

    static std::string getGlyph(const std::string& c, bool before, bool after) {
        static std::unordered_map<std::string, Glyph> map = {
            {"ا", {"ا", "ﺎ", "ﺎ", "ا"}}, {"أ", {"أ", "ﺄ", "ﺄ", "أ"}}, {"إ", {"إ", "ﺈ", "ﺈ", "إ"}}, {"آ", {"آ", "ﺂ", "ﺂ", "آ"}},
            {"ب", {"ﺏ", "ﺐ", "ﺒ", "ﺑ"}}, {"ت", {"ﺕ", "ﺖ", "ﺘ", "ﺗ"}}, {"ث", {"ﺙ", "ﺚ", "ﺜ", "ﺛ"}}, {"ج", {"ﺝ", "ﺞ", "ﺠ", "ﺟ"}},
            {"ح", {"ﺡ", "ﺢ", "ﺤ", "ﺣ"}}, {"خ", {"ﺥ", "ﺦ", "ﺨ", "ﺧ"}}, {"د", {"ﺩ", "ﺪ", "ﺪ", "ﺩ"}}, {"ذ", {"ﺫ", "ﺬ", "ﺬ", "ﺫ"}},
            {"ر", {"ﺭ", "ﺮ", "ﺮ", "ﺭ"}}, {"ز", {"ﺯ", "ﺰ", "ﺰ", "ﺯ"}}, {"س", {"ﺱ", "ﺲ", "ﺴ", "ﺳ"}}, {"ش", {"ﺵ", "ﺶ", "ﺸ", "ﺷ"}},
            {"ص", {"ﺹ", "ﺺ", "ﺼ", "ﺻ"}}, {"ض", {"ﺽ", "ﺾ", "ﻀ", "ﺿ"}}, {"ط", {"ﻁ", "ﻂ", "ﻄ", "ﻃ"}}, {"ظ", {"ﻅ", "ﻆ", "ﻈ", "ﻇ"}},
            {"ع", {"ﻉ", "ﻊ", "ﻌ", "ﻋ"}}, {"غ", {"ﻍ", "ﻎ", "ﻐ", "ﻏ"}}, {"ف", {"ﻑ", "ﻒ", "ﻔ", "ﻓ"}}, {"ق", {"ﻕ", "ﻖ", "ﻘ", "ﻗ"}},
            {"ك", {"ﻙ", "ﻚ", "ﻜ", "ﻛ"}}, {"ل", {"ﻝ", "ﻞ", "ﻠ", "ﻟ"}}, {"م", {"ﻡ", "ﻢ", "ﻤ", "ﻣ"}}, {"ن", {"ﻥ", "ﻦ", "ﻨ", "ﻧ"}},
            {"ه", {"ﻩ", "ﻪ", "ﻬ", "ﻫ"}}, {"و", {"ﻭ", "ﻮ", "ﻮ", "ﻭ"}}, {"ي", {"ﻱ", "ﻲ", "ﻴ", "ﻳ"}}, {"ة", {"ﺓ", "ﺔ", "ﺔ", "ﺓ"}},
            {"ى", {"ﻯ", "ﻰ", "ﻰ", "ﻯ"}}, {"ئ", {"ﺉ", "ﺊ", "ﺌ", "ﺋ"}}, {"ؤ", {"ﺅ", "ﺆ", "ﺆ", "ﺅ"}}, {"ء", {"ﺀ", "ﺀ", "ﺀ", "ﺀ"}}
        };

        if (map.find(c) != map.end()) {
            if (before && after) return map[c].middle;
            if (before && !after) return map[c].end;
            if (!before && after) return map[c].beginning;
            return map[c].isolated;
        }
        return c; 
    }
};

#endif // ARABIC_RESHAPER_H