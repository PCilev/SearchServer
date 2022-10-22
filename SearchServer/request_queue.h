#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(string_view raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(string_view raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(string_view raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::vector<Document> result;
        int number;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int curr_in_day = 0;
    int sum_no_result = 0;
    const SearchServer& server;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(string_view raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> results = server.FindTopDocuments(raw_query, document_predicate);
    ++curr_in_day;
    if (results.empty()) ++sum_no_result;
    if (requests_.size() >= min_in_day_) {
        requests_.pop_front();
        requests_.push_back(QueryResult{ results, curr_in_day % min_in_day_ });
        if (requests_[0].result.empty()) --sum_no_result;
        return results;
    }
    requests_.push_back(QueryResult{ results, curr_in_day });
    return results;
 }