#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view text)
{
    std::vector<std::string_view> words;
    
    std::string_view word;
    
    int w_begin = 0;

    while(w_begin <= text.length()) {
        int w_end = text.find(' ', w_begin);

        words.push_back(text.substr(w_begin, w_end - w_begin));
        w_begin = (w_end == std::string_view::npos)
            ? w_end
            : w_end + 1;
    }
    return words;
}
