#pragma once
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>
#include <thread>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;
const unsigned int CONCURRENT_THREADS = thread::hardware_concurrency();

class SearchServer {
    
public:
    
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const string& stop_words_text);
    explicit SearchServer(string_view stop_words_text);

    void AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings);

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const;
    vector<Document> FindTopDocuments(string_view raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(string_view raw_query) const;

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentPredicate document_predicate) const;
    vector<Document> FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(const execution::sequenced_policy&, string_view raw_query) const;
    
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentPredicate document_predicate) const;
    vector<Document> FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(const execution::parallel_policy&, string_view raw_query) const;

    int GetDocumentCount() const;

    set<int>::const_iterator begin() const;

    set<int>::const_iterator end() const;

    const map<string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

    tuple<vector<string_view>, DocumentStatus> MatchDocument(string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(const execution::sequenced_policy&, string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(const execution::parallel_policy&, string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        string text;
    };
    const set<string, less<>>stop_words_;
    map<string_view, map<int, double>> word_to_document_freqs_;
    map<int, map<string_view, double>> document_to_word_freqs_;
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
        vector<string_view> plus_words;
        vector<string_view> minus_words;
    };

    Query ParseQuery(string_view text, bool skip_sort = false) const;

    double ComputeWordInverseDocumentFreq(string_view word) const;

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};



template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(execution::seq, raw_query, document_predicate);
}

template<typename DocumentPredicate>
inline vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentPredicate document_predicate) const
{
    
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
        
    return matched_documents;
}

template<typename DocumentPredicate>
inline vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(execution::par, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(execution::seq, query, document_predicate);
}

template<typename DocumentPredicate>
inline vector<Document> SearchServer::FindAllDocuments(const execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const
{
    map<int, double> document_to_relevance;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename DocumentPredicate>
inline vector<Document> SearchServer::FindAllDocuments(const execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> tmp(CONCURRENT_THREADS);

    for_each(execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [&](string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {

                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        tmp[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        
        });
    map<int, double> document_to_relevance = tmp.BuildOrdinaryMap();

    for_each(execution::par,
        query.minus_words.begin(), query.minus_words.end(),
        [&](string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(document_id);
                }
            }
        });

    vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance.size());
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    
    return matched_documents;
}


template<typename ExecutionPolicy>
inline void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id)
{
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    documents_.erase(document_id);
    document_ids_.erase(document_id);

    auto& words_freqs = document_to_word_freqs_.at(document_id);
    vector<const string*> v_words_freqs(words_freqs.size());
    transform(
        words_freqs.begin(), words_freqs.end(),
        v_words_freqs.begin(),
        [&](auto& word_freq) {return &(word_freq.first);}
    );

    for_each(policy,
        v_words_freqs.begin(), v_words_freqs.end(),
        [&](auto& word) {
            word_to_document_freqs_.at(*word).erase(document_id);
        });

    document_to_word_freqs_.erase(document_id);
}