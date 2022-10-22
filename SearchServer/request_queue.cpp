#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : server{ search_server } {}

 std::vector<Document> RequestQueue::AddFindRequest(string_view raw_query, DocumentStatus status) {
        return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
}

std::vector<Document> RequestQueue::AddFindRequest(string_view raw_query) {
        return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
        return sum_no_result;
}