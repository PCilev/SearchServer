#include "test_example_functions.h"

void AddDocument(SearchServer& search_server, int document_id, string_view document, 
            DocumentStatus status, const vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}