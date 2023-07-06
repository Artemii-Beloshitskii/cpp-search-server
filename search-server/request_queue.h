#pragma once

#include <queue>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    std::deque<int> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_request;
    int no_result;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const std::vector<Document> request_ = search_server_request.FindTopDocuments(raw_query, document_predicate);
    if (request_.empty()) {
        no_result++;
    }
    if (requests_.size() >= min_in_day_) {
        requests_.pop_front();
        no_result--;
    }
    requests_.push_front(static_cast<int>(request_.size()));
    return request_;
}