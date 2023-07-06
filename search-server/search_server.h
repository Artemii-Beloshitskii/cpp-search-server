#pragma once

#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <execution>
#include <string_view>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std::string_literals;

const double MAX_DIFFERENCE = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
        const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    typename std::set<int>::const_iterator begin() const;

    typename std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::parallel_policy& p_p, int document_id);

    void RemoveDocument(const std::execution::sequenced_policy& s_p, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
        int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy& p_p,
        std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& s_p, std::string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    std::set<std::string, std::less<>> stop_words_;
    std::set<std::string, std::less<>> storage;

    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> word_frequencies_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text, bool seq = 1) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy policy, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {

    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    std::sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < MAX_DIFFERENCE) {
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
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    
    const auto query = ParseQuery(raw_query);
    
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < MAX_DIFFERENCE) {
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
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {

    std::map<int, double> document_to_relevance;

    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.find(word)->second) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.find(word)->second) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy policy, const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy, const Query& query, DocumentPredicate document_predicate) const {
    const int concurrent_map_size = 100;
    ConcurrentMap<int, double> document_to_relevance_help(concurrent_map_size);

    ForEach(policy, query.plus_words, [this, &document_to_relevance_help, &document_predicate](std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto& [document_id, term_freq] : word_to_document_freqs_.find(word)->second) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance_help[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });

    ForEach(policy, query.minus_words, [this, &document_to_relevance_help](std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto& [document_id, _] : word_to_document_freqs_.find(word)->second) {
                    document_to_relevance_help.Erase(document_id);
                }
            }
        });

    auto document_to_relevance = document_to_relevance_help.BuildOrdinaryMap();

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }

    return matched_documents;
}