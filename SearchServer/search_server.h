#pragma once
#include "document.h"
#include "string_processing.h"
#include "read_input_functions.h"
#include "concurrent_map.h"

#include <map>
#include <algorithm>
#include <stdexcept>
#include <execution>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-06;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings);

    template <typename ExecutionPolicy, typename DocumentPredicate>
    vector<Document> FindTopDocuments(ExecutionPolicy& policy, string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const {
        return FindTopDocuments(execution::seq, raw_query, document_predicate);
    }
    
    template <typename ExecutionPolicy>
    vector<Document> FindTopDocuments(ExecutionPolicy& policy, string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(policy, raw_query, [&status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
    }
    vector<Document> FindTopDocuments(string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(execution::seq, raw_query, status);
    }
        
    template <typename ExecutionPolicy>
    vector<Document> FindTopDocuments(ExecutionPolicy& policy, string_view raw_query) const {
        return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
    }
    vector<Document> FindTopDocuments(string_view raw_query) const {
        return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
    }
    
    int GetDocumentCount() const;
    
    const map<string_view, double> GetWordFrequencies(int document_id) const;
    
    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id) {
        const map<string_view, double>& word_freqs = GetWordFrequencies(document_id);
        vector<string_view> temp;
        for(const auto& word_freq : word_freqs) {
            temp.push_back(word_freq.first);
        }
        
        for_each(policy, temp.cbegin(), temp.cend(),
                 [&word_freq = word_to_document_freqs_, &document_id](string_view par) { 
                    if (word_freq[static_cast<string>(par)].size() > 0) {
                        word_freq[static_cast<string>(par)].erase(document_id);
                    }
                });
       
        documents_.erase(document_id);
        word_frequencies_.erase(document_id);
        document_ids_.erase(document_id);
    }
    
    void RemoveDocument(int document_id) {
        RemoveDocument(execution::seq, document_id);
    };
    
    set<int>::iterator begin();
    set<int>::iterator end();
    
    tuple<vector<string_view>, DocumentStatus> MatchDocument(execution::parallel_policy, string_view raw_query, int document_id) const {
        const auto query = ParseQuery(raw_query);
        vector<string_view> matched_words;
        const map<string_view, double>& word_freq = GetWordFrequencies(document_id);
        
        auto it = find_if(query.minus_words.begin(), query.minus_words.end(),
                            [&word_freq](string_view word){
                                return word_freq.count(word); 
                          });
        
        if (it != query.minus_words.end()) {
            return { matched_words, documents_.at(document_id).status }; 
        }
        
        copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), back_inserter(matched_words),
                [&word_freq](string_view word){
                    return word_freq.count(word);
                });
        
        std::sort(execution::par, matched_words.begin(), matched_words.end());
        auto last = std::unique(execution::par, matched_words.begin(), matched_words.end());
        matched_words.erase(last, matched_words.end());
        
        return { matched_words, documents_.at(document_id).status };
    }

    tuple<vector<string_view>, DocumentStatus> MatchDocument(string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(execution::sequenced_policy, string_view raw_query, int document_id) const {
        return MatchDocument(raw_query, document_id);
    }
           
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string, less<>> stop_words_;
    map<string, map<int, double>, less<>> word_to_document_freqs_;
    map<int, map<string, double, less<>>> word_frequencies_;
    map<int, DocumentData> documents_;
    set<int> document_ids_;

    bool IsStopWord(string_view word) const;
    static bool IsValidWord(string_view word);
    vector<string_view> SplitIntoWordsNoStop(string_view text) const;
    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWord {
        string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string_view text) const;

    struct Query {
        void RemoveDuplicates() {
            Remove(minus_words);
            Remove(plus_words);
        }
        
        vector<string_view> plus_words;
        vector<string_view> minus_words;
    private:
        void Remove(vector<string_view>& vec) {
            sort(vec.begin(), vec.end());
            auto last = unique(vec.begin(), vec.end());
            vec.erase(last, vec.end());
        }
    };

    Query ParseQuery(string_view text) const;
    
    double ComputeWordInverseDocumentFreq(string_view word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    vector<Document> FindAllDocuments(ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const;
   
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) 
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw invalid_argument("Some of stop words are invalid");
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy& policy, string_view raw_query,
                                                DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query); 
    query.RemoveDuplicates();

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename ExecutionPolicy, typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const {
    //size_t NUM = 10;
    ConcurrentMap<int, double> document_to_relevance(document_ids_.size());
    for_each(policy, query.plus_words.begin(), query.plus_words.end(), 
    [this, &document_to_relevance, document_predicate](string_view word){
        if (word_to_document_freqs_.count(word)) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(static_cast<string>(word))) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    });
    
    for_each(policy, query.minus_words.begin(), query.minus_words.end(),
    [this, &document_to_relevance](string_view word){
        if (word_to_document_freqs_.count(word)) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(static_cast<string>(word))) {
                document_to_relevance.Erase(document_id);
            }
        }
    });

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    
    return matched_documents;
}