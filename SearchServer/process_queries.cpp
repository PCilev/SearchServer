#include <numeric>
#include <execution>
#include <utility>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> documents_lists(queries.size());
    
    transform(execution::par, queries.begin(), queries.end(), documents_lists.begin(),
              [&search_server](string_view str) { return search_server.FindTopDocuments(execution::par, str); });
    return documents_lists;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> documents_lists = ProcessQueries(search_server, queries);

    std::vector<Document> documents;
    for (auto& docs : documents_lists) {
        documents.insert(documents.end(), docs.begin(), docs.end());
    }
    return documents;
}