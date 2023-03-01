#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAX_DIFFERENCE = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings),status });
    }

    template <typename Extra_Fun>
    vector<Document> FindTopDocuments(const string& raw_query, Extra_Fun extra_fun) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, extra_fun);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < MAX_DIFFERENCE) {
                    return lhs.rating > rhs.rating;
                }
        return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_ = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status_](int document_id, DocumentStatus status, int rating)
            { return status == status_; });
    }


    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Extra_Fun>
    bool SortBy(const int& id, const Extra_Fun& extra_fun) const {
        const auto& needed_doc = documents_.at(id);
        if (extra_fun(id, needed_doc.status, needed_doc.rating)) {
            return true;
        }
        return false;
    }

    template <typename Extra_Fun>
    vector<Document> FindAllDocuments(const Query& query, Extra_Fun extra_fun) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (SortBy(document_id, extra_fun)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }


};

//НА ПРОВЕРКУ

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestMinusWords() {
    const int doc_id = 1;
    const string content = "cat city"s;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT(server.FindTopDocuments("city -cat"s).size() == 0);
}

void TestMatch() {
    const int doc_id = 1;
    const string content = "small cat"s;
    const vector<int> ratings = { 1, 2, 3 };
    const int doc_id_2 = 2;
    const string content_2 = "big dog"s;
    const vector<int> ratings_2 = { 5, 20, 5 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    const auto& [matched_words, status] = server.MatchDocument("small cat big dog"s, 1);
    const vector<string> real_matched_words = { "cat"s, "small"s };
    ASSERT(matched_words == real_matched_words);
}

void TestRelRait() {
    const int doc_id = 1;
    const string content = "small cat"s;
    const vector<int> ratings = { 1, 2, 3 };
    const int doc_id_2 = 2;
    const string content_2 = "big dog"s;
    const vector<int> ratings_2 = { 5, 20, 5 };
    const int doc_id_3 = 3;
    const string content_3 = "small cat dog"s;
    const vector<int> ratings_3 = { 2, 4, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
    const auto& rel = server.FindTopDocuments("cat dog small");
    ASSERT(rel[0].id == 3 && rel[1].id == 1 && rel[2].id == 2);
    ASSERT(rel[0].rating == 3 && rel[1].rating == 2 && rel[2].rating == 10);
}

void TestPredicatandStatus() {
    const int doc_id = 1;
    const string content = "small cat"s;
    const vector<int> ratings = { 1, 2, 3 };
    const int doc_id_2 = 2;
    const string content_2 = "big dog"s;
    const vector<int> ratings_2 = { 5, 20, 5 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings_2);
    {
        const auto& rel = server.FindTopDocuments("cat dog small", [](int document_id, DocumentStatus status, int rating) {
            return status == DocumentStatus::BANNED;
            });
        ASSERT(rel[0].id == 2);
    }
    {
        const auto& rel = server.FindTopDocuments("cat dog small", DocumentStatus::BANNED);
        ASSERT(rel[0].id == 2);
    }
}

void TestRelCalc() {
    const int doc_id = 1;
    const string content = "small cat"s;//0.693147
    const vector<int> ratings = { 1, 2, 3 };
    const int doc_id_2 = 2;
    const string content_2 = "big dog"s;//0.346574
    const vector<int> ratings_2 = { 5, 20, 5 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    const auto& rel_calc = server.FindTopDocuments("cat dog small");
    ASSERT((rel_calc[0].relevance - 0.693147) < MAX_DIFFERENCE);
}

#define RUN_TEST(Function) Function()

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatch);
    RUN_TEST(TestRelRait);
    RUN_TEST(TestPredicatandStatus);
    RUN_TEST(TestRelCalc);
}

//НА ПРОВЕРКУ

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}


int main() {
    TestSearchServer();
    SearchServer search_server;

    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    cout << "ACTUAL by default:"s << endl;

    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}
