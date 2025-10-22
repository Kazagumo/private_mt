#pragma once
#include <string>
#include <vector>

struct ResultItem {
    std::string word;
    uint32_t weight;
};

// 初始化索引（会扫描 py_entries）
void build_pinyin_index();

// 搜索，返回按权重降序的最多 max_results 个候选
std::vector<ResultItem> search_pinyin(const std::string &input, size_t max_results = 10);

// 自动分词：将无空格或连写拼音切分为音节列表
std::vector<std::string> tokenize_auto(const std::string &input);
