#pragma once
#include <iostream>

template <typename It>
class IteratorRange {
public:
    IteratorRange(It range_begin, It range_end) :
        range_begin_(range_begin),
        range_end_(range_end),
        size_(distance(range_begin_, range_end_)) {

    }
    It begin() const {
        return range_begin_;
    }
    It end() const {
        return range_end_;
    }
    int size() const {
        return size_;
    }

private:
    It range_begin_;
    It range_end_;
    size_t size_;
};


template <typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size)
    {
        for (size_t range_size = distance(begin, end);range_size > 0;) {
            const size_t current_page_size = min(page_size, range_size);
            const It current_page_end = next(begin, current_page_size);
            pages_.push_back({ begin, current_page_end });
            range_size -= current_page_size;
            begin = current_page_end;
        }
    }
    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    size_t size() const {
        return pages_.size();
    }
private:
    std::vector<IteratorRange<It>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename It>
std::ostream& operator << (std::ostream& output, IteratorRange<It> it) {
    for (auto iter = it.begin(); iter != it.end(); iter++) {
        output << *iter;
    }
    return output;
}