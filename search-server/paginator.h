#pragma once

#include <vector>
#include <iostream>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end) :
        page_begin(begin),
        page_end(end)
    {
    }
    const Iterator begin() const {
        return page_begin;
    }
    const Iterator end() const {
        return page_end;
    }
private:
    Iterator page_begin, page_end;
};

template <typename Iterator>
std::ostream& operator<< (std::ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator iterator = range.begin(); iterator < range.end(); ++iterator) {
        out << *iterator;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t current_begin = std::distance(begin, end); current_begin > 0;) {
            if (current_begin < page_size) {
                all_pages.push_back({ begin, std::next(begin, current_begin) });
                begin = std::next(begin, current_begin);
                current_begin -= current_begin;
            }
            else {
                all_pages.push_back({ begin, std::next(begin, page_size) });
                begin = std::next(begin, page_size);
                current_begin -= page_size;
            }
        }
    }
    typename std::vector<IteratorRange<Iterator>>::const_iterator size() const {
        return all_pages.size();
    }
    typename std::vector<IteratorRange<Iterator>>::const_iterator begin() const {
        return all_pages.begin();
    }
    typename std::vector<IteratorRange<Iterator>>::const_iterator end() const {
        return all_pages.end();
    }
private:
    std::vector<IteratorRange<Iterator>> all_pages;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}