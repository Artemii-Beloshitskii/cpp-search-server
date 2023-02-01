#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        for (const string& word : words) {
            word_id[word][document_id] += 1.0 / words.size(); //исправил подсчёт tf
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    void DocumentCount(const int& number) {
        document_count = number;
    }

private:

    struct query {
        set<string> plus_w;
        set<string> minus_w;
    };

    map<string, map<int, double>> word_id;

    set<string> stop_words_;

    double document_count = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    query ParseQuery(const string& text) const {
        query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                word.substr(1);
                query_words.minus_w.insert(word);
            }
            else {
                query_words.plus_w.insert(word);
            }
        }
        return query_words;
    }

    static double IDF_cal(const double& doc_count, const int& doc_size) {
        return log(doc_count / doc_size);
    }

    vector<Document> FindAllDocuments(const query& query_words) const {
        map<int, double> doc_rel;

        for (const string& word : query_words.plus_w) {
            if (word_id.count(word) != 0) {
                double idf = IDF_cal(document_count, word_id.at(word).size());

                for (const pair<int, double>& id : word_id.at(word)) {
                    doc_rel[id.first] += idf * id.second; //исправил подсчёт idf
                }
            }
        }
        for (const string& word : query_words.minus_w) {
            if (word_id.count('-' + word) != 0) {
                for (const auto& id : word_id.at(word)) {
                    doc_rel.erase(id.first);
                }
            }
        }
        vector<Document> matched_documents;
        for (const auto& id : doc_rel) {
            matched_documents.push_back({ id.first, id.second });
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    search_server.DocumentCount(document_count);
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& document_relevance : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_relevance.id << ", " << "relevance = "s << document_relevance.relevance << " }"s << endl;
    }
}