#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> ret_vec(queries.size());
    
    std::transform(std::execution::par, queries.begin(), queries.end(), ret_vec.begin(), 
        [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); });
    
    return ret_vec;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<Document> ret_vec_int{};

    for (std::vector<Document>& vec_doc : ProcessQueries(search_server, queries)) {
        for (Document& doc : vec_doc) {
            ret_vec_int.push_back(doc);
        }
    }

    return ret_vec_int;
}
