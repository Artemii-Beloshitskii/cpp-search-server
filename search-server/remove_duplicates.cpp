#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server)
{
    std::map<std::set<std::string>, int> anti_duplicate_map;
    std::set<int> anti_duplicate_id;

    for (const int id : search_server) {
        std::map<std::string, double> word_frequencies_internal = search_server.GetWordFrequencies(id);
        std::set<std::string> words_internal;
        for (const auto& word : word_frequencies_internal) {
            words_internal.insert(word.first);
        }
        if (anti_duplicate_map.count(words_internal) != 0) {
            int current_id = anti_duplicate_map.at(words_internal);
            anti_duplicate_id.insert(std::max(current_id, id));
            anti_duplicate_map[words_internal] = std::min(current_id, id);
        }
        else {
            anti_duplicate_map.insert(std::pair{words_internal,id });
        }
    }

    for (int document_id : anti_duplicate_id) {
        std::cout << "Found duplicate document id " << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}
