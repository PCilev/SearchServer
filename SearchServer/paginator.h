#pragma once
#include <iostream>
#include <algorithm>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator first, Iterator last) : first_(first), last_(last),
        range(distance(first, last)) {}
    Iterator begin() {
        return first_;
    }
    Iterator end() {
        return last_;
    }
    size_t size() {
        return range;
    }

private:
    Iterator first_;
    Iterator last_;
    int64_t range;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator first, Iterator last, size_t page_size) : size_(page_size) {
        int n = distance(first, last) / page_size;
        for (int i = 0; i < n; ++i) {
            ranges.push_back(IteratorRange<Iterator>(first, first + page_size));
            advance(first, page_size);
        }
        if (distance(first, last) % page_size) {
            ranges.push_back(IteratorRange<Iterator>(first, last));
        }
    }

    auto begin() const {
        return ranges.begin();
    }
    auto end() const {
        return ranges.end();
    }
    size_t size() const {
        return ranges.size();
    }

private:
    size_t size_;
    vector<IteratorRange<Iterator>> ranges;
};

template <typename Iterator>
ostream& operator<<(ostream& os, IteratorRange<Iterator> it_r) {
    for (auto first = it_r.begin(); first != it_r.end(); ++first) {
        cout << *first;
    }
    return os;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}