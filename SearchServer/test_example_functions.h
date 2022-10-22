#pragma once
#include "search_server.h"

void AddDocument(SearchServer& search_server, int document_id, string_view document, 
            DocumentStatus status, const vector<int>& ratings);