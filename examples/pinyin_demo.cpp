#include "../src/pinyin_search.h"
#include <iostream>

int main()
{
    build_pinyin_index();
    std::string input;
    std::cout << "Enter pinyin: ";
    std::getline(std::cin, input);
    auto res = search_pinyin(input, 10);
    std::cout << "Top " << res.size() << " results:\n";
    for (size_t i = 0; i < res.size(); ++i) {
        std::cout << i + 1 << ". " << res[i].word << " (" << res[i].weight << ")\n";
    }
    return 0;
}
