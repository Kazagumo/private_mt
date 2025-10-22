#include "pinyin_search.h"
#include "pinyin_data.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

// 声母表（从长到短，以便最长匹配）
static const std::vector<std::string> INITIALS = {"zh", "ch", "sh", "b", "p", "m", "f", "d", "t", "n", "l", "g",
                                                  "k",  "h",  "j",  "q", "x", "r", "z", "c", "s", "y", "w"};

// 常用韵母列表（使用 v 代表 ü），按长度降序以便最长匹配
static const std::vector<std::string> FINALS = {"iang", "uang", "iong", "uang", "uang", "iong", "uai", "uan", "ang", "eng", "ing",
                                                "ong",  "iao",  "ian",  "uan",  "uen",  "vean", "ve",  "ai",  "ei",  "ui",  "ao",
                                                "ou",   "iu",   "ie",   "er",   "ia",   "ua",   "uo",  "uo",  "ia",  "an",  "en",
                                                "in",   "un",   "vn",   "a",    "o",    "e",    "i",   "u",   "v"};

// 将输入规范化为小写字母和空格
static std::string norm(const std::string &s)
{
    std::string out;
    for (char c : s) {
        if (std::isalpha((unsigned char)c) || c == ' ')
            out.push_back(std::tolower((unsigned char)c));
    }
    return out;
}

// 以最长匹配方式把连续无空格的拼音字符串拆分为音节
static std::vector<std::string> split_pinyin_nospace(const std::string &s)
{
    std::string t = norm(s);
    // 如果包含空格，则直接按空格分
    if (t.find(' ') != std::string::npos) {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < t.size()) {
            while (i < t.size() && t[i] == ' ')
                i++;
            if (i >= t.size())
                break;
            size_t j = i;
            while (j < t.size() && t[j] != ' ')
                j++;
            out.push_back(t.substr(i, j - i));
            i = j;
        }
        return out;
    }

    std::vector<std::string> res;
    size_t i = 0;
    while (i < t.size()) {
        bool matched = false;
        // 尝试匹配声母
        for (const auto &ini : INITIALS) {
            if (t.compare(i, ini.size(), ini) == 0) {
                // 找到声母，接着匹配韵母最长后缀
                size_t best_len = 0;
                // 尝试从剩余字符串中取尽可能长的韵母（这里简单取到下一个声母或末尾）
                for (size_t j = t.size(); j > i + ini.size(); --j) {
                    // 最长后缀优先
                    std::string candidate = t.substr(i + ini.size(), j - (i + ini.size()));
                    // 简单验证 candidate 全为字母
                    bool ok = true;
                    for (char c : candidate)
                        if (!isalpha((unsigned char)c)) {
                            ok = false;
                            break;
                        }
                    if (!ok)
                        continue;
                    best_len = candidate.size();
                    break;
                }
                if (best_len == 0)
                    best_len = 1;
                res.push_back(t.substr(i, ini.size() + best_len));
                i += ini.size() + best_len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            // 无声母，直接把尽可能长的一段作为音节（到下个声母或末尾）
            size_t j = i + 1;
            while (j < t.size()) {
                bool next_is_ini = false;
                for (const auto &ini : INITIALS)
                    if (t.compare(j, ini.size(), ini) == 0) {
                        next_is_ini = true;
                        break;
                    }
                if (next_is_ini)
                    break;
                ++j;
            }
            res.push_back(t.substr(i, j - i));
            i = j;
        }
    }
    return res;
}

// 更精确的自动分词，尝试最长匹配声母+韵母
std::vector<std::string> tokenize_auto(const std::string &input)
{
    std::string t = norm(input);
    // 若包含空格，直接按空格分割
    if (t.find(' ') != std::string::npos)
        return split_pinyin_nospace(t);

    std::vector<std::string> res;
    size_t i = 0;
    while (i < t.size()) {
        bool matched = false;
        // 尝试匹配声母
        for (const auto &ini : INITIALS) {
            if (t.compare(i, ini.size(), ini) == 0) {
                // 找到声母，尝试匹配最长韵母
                size_t best_len = 0;
                std::string best_fin;
                for (const auto &fin : FINALS) {
                    if (i + ini.size() + fin.size() <= t.size()) {
                        if (t.compare(i + ini.size(), fin.size(), fin) == 0) {
                            if (fin.size() > best_len) {
                                best_len = fin.size();
                                best_fin = fin;
                            }
                        }
                    }
                }
                if (best_len == 0) {
                    // 没有匹配韵母：
                    // 如果后面还有字符且后续位置是另一个声母，则把当前声母单独作为音节（例如 "by" -> "b" "y").
                    size_t next_pos = i + ini.size();
                    bool make_zero = false;
                    if (next_pos < t.size()) {
                        for (const auto &ini2 : INITIALS) {
                            if (t.compare(next_pos, ini2.size(), ini2) == 0) {
                                make_zero = true;
                                break;
                            }
                        }
                    } else {
                        // 已到结尾，只有声母，作为单个音节
                        make_zero = true;
                    }
                    if (make_zero) {
                        best_len = 0;
                    } else {
                        // 否则退回用单字符做韵母，但确保不越界
                        best_len = (next_pos < t.size()) ? 1 : 0;
                    }
                }
                res.push_back(t.substr(i, ini.size() + best_len));
                i += ini.size() + best_len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            // 没有声母，尝试匹配最长韵母
            size_t best_len = 0;
            for (const auto &fin : FINALS) {
                if (i + fin.size() <= t.size() && t.compare(i, fin.size(), fin) == 0) {
                    if (fin.size() > best_len)
                        best_len = fin.size();
                }
            }
            if (best_len == 0)
                best_len = 1;
            res.push_back(t.substr(i, best_len));
            i += best_len;
        }
    }
    return res;
}

// 预索引：按音节数量和首字母索引条目索引数组
static std::unordered_map<int, std::unordered_map<char, std::vector<const pyentry *>>> index_map;

void build_pinyin_index()
{
    index_map.clear();
    for (uint32_t i = 0; i < py_entries_count; ++i) {
        const pyentry &e = py_entries[i];
        // 计算音节数
        std::string inits = e.initials ? std::string(e.initials) : std::string();
        int sylls = 0;
        for (size_t p = 0; p < inits.size();) {
            while (p < inits.size() && inits[p] == ' ')
                p++;
            if (p >= inits.size())
                break;
            size_t q = p;
            while (q < inits.size() && inits[q] != ' ')
                q++;
            sylls++;
            p = q;
        }
        char first_char = '_';
        for (char c : inits)
            if (c != ' ') {
                first_char = c;
                break;
            }
        index_map[sylls][first_char].push_back(&e);
    }
}

// 检查条目是否以 sylls 作为前缀（使用 initials 的首字母比较）
static bool matches_prefix_entry(const pyentry &e, const std::vector<std::string> &sylls)
{
    std::string inits = e.initials ? std::string(e.initials) : std::string();
    std::vector<std::string> entry_init;
    size_t i = 0;
    while (i < inits.size()) {
        while (i < inits.size() && inits[i] == ' ')
            i++;
        if (i >= inits.size())
            break;
        size_t j = i;
        while (j < inits.size() && inits[j] != ' ')
            j++;
        entry_init.push_back(inits.substr(i, j - i));
        i = j;
    }
    if (entry_init.size() < sylls.size())
        return false;
    for (size_t k = 0; k < sylls.size(); ++k) {
        if (sylls[k].empty())
            return false;
        char a = sylls[k][0]; // input first letter
        if (entry_init[k].empty() || entry_init[k][0] == '_')
            return false;
        if (entry_init[k][0] != a)
            return false;
    }
    return true;
}

std::vector<ResultItem> search_pinyin(const std::string &input, size_t max_results)
{
    if (index_map.empty())
        build_pinyin_index();
    auto sylls = tokenize_auto(input);
    std::vector<ResultItem> results;
    int scount = (int)sylls.size();
    char first_char = '_';
    for (char c : sylls[0])
        if (c != ' ') {
            first_char = c;
            break;
        }

    // 搜索时优先：完全相同音节数的组 -> 更多音节的组
    std::vector<int> search_counts;
    search_counts.push_back(scount);
    for (int d = 1; d <= 3; ++d)
        search_counts.push_back(scount + d);

    // collect matching pyentry pointers
    std::vector<const pyentry *> matches;
    for (int cnt : search_counts) {
        auto it1 = index_map.find(cnt);
        if (it1 == index_map.end())
            continue;
        auto it2 = it1->second.find(first_char);
        if (it2 == it1->second.end())
            continue;
        for (const pyentry *pe : it2->second) {
            if (matches_prefix_entry(*pe, sylls)) {
                matches.push_back(pe);
            }
        }
    }
    // 如果输入是多音节（通常代表多字拼音），过滤掉单字结果
    auto utf8_char_count = [](const char *s) -> size_t {
        if (!s)
            return 0;
        size_t cnt = 0;
        const unsigned char *p = (const unsigned char *)s;
        while (*p) {
            // UTF-8 continuation bytes have form 10xxxxxx (0x80..0xBF)
            if ((*p & 0xC0) != 0x80)
                cnt++;
            p++;
        }
        return cnt;
    };

    bool input_is_multisyll = (scount > 1);
    // build result items with filtering
    for (const pyentry *pe : matches) {
        if (input_is_multisyll) {
            size_t chars = utf8_char_count(pe->data);
            if (chars == 1)
                continue; // 丢弃单字结果
        }
        results.push_back({std::string(pe->data), pe->frequent});
    }

    // 按权重排序并截取
    std::sort(results.begin(), results.end(), [](const ResultItem &a, const ResultItem &b) { return a.weight > b.weight; });
    if (results.size() > max_results)
        results.resize(max_results);
    return results;
}
