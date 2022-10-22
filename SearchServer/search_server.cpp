#include "search_server.h"
#include <cmath>
#include <numeric>

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) 
{
}

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) 
{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (string_view word : words) {
        word_to_document_freqs_[static_cast<string>(word)][document_id] += inv_word_count;
        word_frequencies_[document_id][static_cast<string>(word)] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

/*vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [&status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}*/

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    auto query = ParseQuery(raw_query);
    query.RemoveDuplicates();
        
    vector<string_view> matched_words;
    map<string_view, double> word_freq = GetWordFrequencies(document_id);
    
    /*for (string_view word : query.minus_words) {
        if (word_freq.count(word)) {
            return { matched_words, documents_.at(document_id).status };
        }
    }
    
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(static_cast<string>(word)).count(document_id)) {
            matched_words.push_back(word);
        }
    }*/
    
    auto it = find_if(query.minus_words.begin(), query.minus_words.end(),
                            [&word_freq](string_view word){
                                return word_freq.count(word); 
                          });
        
    if (it != query.minus_words.end()) {
        return { matched_words, documents_.at(document_id).status }; 
    }
        
    copy_if(query.plus_words.begin(), query.plus_words.end(), back_inserter(matched_words),
            [&word_freq](string_view word){
                return word_freq.count(word);
            });

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) { 
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + static_cast<string>(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

 SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + static_cast<string>(text) + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    SearchServer::Query result;
    const auto words = SplitIntoWords(text);
    for_each(words.begin(), words.end(), [this, &result](string_view word){
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    });
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(static_cast<string>(word)).size());
}

const map<string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    auto result = word_frequencies_.find(document_id);
    if (result != word_frequencies_.end()) {
        return map<string_view, double>(result->second.begin(), result->second.end());
    }
    static map<string_view, double> empty_map;
    return empty_map;
}

set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}