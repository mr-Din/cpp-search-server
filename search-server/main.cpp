#include <cassert>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <cstdlib>
#include <iomanip>
#include <Windows.h>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

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
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    };

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto status_lambda = [status](int document_id, DocumentStatus status_, int rating) {
            return status_ == status;
        };
        return FindTopDocuments(raw_query, status_lambda);
    }

    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        Predicate predicate) const {

        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
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
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
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

    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (predicate(document_id, document_data.status, document_data.rating)) {
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


void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

template<typename T>
ostream& operator<<(ostream& os, const vector<T>& container) {

    if (container.empty()) {
        os << "Empty"s;
    }
    else {
        os << "[";
        for (const T& item : container) {
            os << item;
            if (item != container.back()) {
                os << ", "s;
            }
        }
        os << "]";
    }
    return os;
}

template<typename T>
ostream& operator<<(ostream& os, const set<T>& container) {

    if (container.empty()) {
        os << "Empty"s;
    }
    else {
        os << "{";
        int count = container.size();
        for (const T& item : container) {
            os << item;
            --count;
            if (count > 0) {
                os << ", "s;
            }
        }
        os << "}";
    }
    return os;
}

template<typename T1, typename T2>
ostream& operator<<(ostream& os, const map<T1, T2>& container) {

    if (container.empty()) {
        os << "Empty"s;
    }
    else {
        os << "{";
        int count = container.size();
        for (const auto& [key, value] : container) {
            os << key << ": " << value;
            --count;
            if (count > 0) {
                os << ", "s;
            }
        }
        os << "}";
    }
    return os;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str,
    const string& file, const string& func, unsigned line, const string& hint) {
    
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

template <typename T>
void RunTestImpl(const T& t, const string& func) {
    cerr << func << " OK" << endl;
    t();
}

#define RUN_TEST(func) RunTestImpl((func),#func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
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

// Тест проверяет, что добавленный документ находится по поисковому запросу,
// который содержит слова из этого документа
void TestAddedDocumentIsSearchByQuery() {

    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 42;
    const string content_1 = "fox on a table"s;
    const string content_2 = "dog in the town"s;
    const string content_3 = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    const string query = "in the city"s;

    SearchServer server;

    ASSERT_EQUAL(server.FindTopDocuments(query).size(), 0);

    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings);

    const auto found_documents = server.FindTopDocuments(query);

    ASSERT(found_documents.size() > 0u);
    ASSERT(found_documents.size() != 3u);
    ASSERT_EQUAL_HINT(found_documents.size(), 2u, "Count of documents found should be 2"s);

    ASSERT_EQUAL(found_documents[1].id, doc_id_2);
    ASSERT_EQUAL(found_documents[0].id, doc_id_3);
}

// Тест проверяет, что документы с минус-словами не включаются в результаты поиска
void TestNoMinusWordsDocumentsInQuery() {

    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 42;
    const string content_1 = "fox on a table"s;
    const string content_2 = "dog in the town"s;
    const string content_3 = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    const string query1 = "in the -city"s;
    const string query2 = "in the -city -the"s;

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings);

    ASSERT_EQUAL(server.FindTopDocuments(query1).size(), 1);
    ASSERT_HINT(server.FindTopDocuments(query2).empty(), "Document with minus-words should not be included in FindTopDocuments"s);
}

// Тест проверки соответствия документа поиску (matching)
void TestMatchedDocuments() {

    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 42;
    const string content_1 = "fox on a table"s;
    const string content_2 = "dog in the town"s;
    const string content_3 = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    const string query = "in the city"s;

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings);

    vector<string> words_2 = { "in"s,"the"s };
    vector<string> words_3 = { "in"s,"the"s, "city"s };
    vector<string> words_4 = { "city"s, "in"s,"the"s };

    ASSERT_EQUAL_HINT(get<0>(server.MatchDocument(query, doc_id_1)).size(), 0, "Count of words from the query present in the document should be: 0"s);
    ASSERT_EQUAL_HINT(get<0>(server.MatchDocument(query, doc_id_2)).size(), 2, "Count of words from the query present in the document should be: 2"s);
    ASSERT_EQUAL_HINT(get<0>(server.MatchDocument(query, doc_id_3)).size(), 3, "Count of words from the query present in the document should be: 3"s);

    ASSERT_EQUAL(get<0>(server.MatchDocument(query, doc_id_2)), words_2);
    ASSERT(get<0>(server.MatchDocument(query, doc_id_3)) != words_3);
    ASSERT_EQUAL(get<0>(server.MatchDocument(query, doc_id_3)), words_4);
}

// Тест проверки сортировки (по убыванию релевантности)
void TestDocumentsSorting() {
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3, doc_id_4 = 42;
    const string content_1 = "animal fox on the table"s;
    const string content_2 = "pig in the city"s;
    const string content_3 = "cat in the city"s;
    const string content_4 = "animal dog in town"s;
    const vector<int> ratings = { 1, 2, 3 };
    const string query = "animal in the city"s;

    SearchServer server;

    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_4, content_4, DocumentStatus::ACTUAL, ratings);

    const auto found_documents = server.FindTopDocuments(query);

    // Проверка сортировки ф-ей is_sorted
    ASSERT(is_sorted(found_documents.begin(), found_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        }));
    // Проверка сортировки циклом
    bool sort_rigth = true;
    for (int i = 0; i + 1 < static_cast<int> (found_documents.size()); ++i) {
        if (found_documents[i].relevance < found_documents[i + 1].relevance) {
            sort_rigth = false;
        }
    }
    ASSERT(sort_rigth);
}

// Тест проверки правильного вычисления рейтинга документа (среднее арифметическое
// оценок документа)
void TestDocumentRating() {
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3, doc_id_4 = 42, doc_id_5 = 5;
    const string content = "animal fox on the table"s;
    const vector<int> ratings1 = { 1, 2, 3 };       //2
    const vector<int> ratings2 = { 5, 3, 1 };       //3
    const vector<int> ratings3 = { 0, 1 };          //0
    const vector<int> ratings4 = { 0, 2, 10, 4 };   //4
    const vector<int> ratings5 = { 1, 0, 6, 0 };    //1

    SearchServer server;
    server.AddDocument(doc_id_1, content, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id_2, content, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id_3, content, DocumentStatus::ACTUAL, ratings3);
    server.AddDocument(doc_id_4, content, DocumentStatus::ACTUAL, ratings4);
    server.AddDocument(doc_id_5, content, DocumentStatus::ACTUAL, ratings5);

    const auto top_documents = server.FindTopDocuments("fox");

    ASSERT_EQUAL(top_documents[0].rating, 4);
    ASSERT_EQUAL(top_documents[1].rating, 3);
    ASSERT_EQUAL(top_documents[2].rating, 2);
    ASSERT_EQUAL(top_documents[3].rating, 1);
    ASSERT_EQUAL(top_documents[4].rating, 0);
}

// Тест проверки фильтрации результатов поиска с использованием предиката
void TestFilteringByPredicate() {
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3, doc_id_4 = 5, doc_id_5 = 6;
    const string content_1 = "animal fox on the table"s;
    const string content_2 = "pig in the city"s;
    const string content_3 = "cat in the city"s;
    const string content_4 = "animal dog in town"s;
    const string content_5 = "animal in town"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const vector<int> ratings2 = { 5, 3, 1 };
    const vector<int> ratings3 = { 0, 1 };
    const vector<int> ratings4 = { 0, 2, 10, 4 };
    const vector<int> ratings5 = { 1, 0, 6, 0 };
    const string query = "animal in the city"s;

    SearchServer server;

    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings2);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings3);
    server.AddDocument(doc_id_4, content_4, DocumentStatus::IRRELEVANT, ratings4);
    server.AddDocument(doc_id_5, content_5, DocumentStatus::ACTUAL, ratings5);

    const auto even_id_documents = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0; });
    const auto rating_more_1_documents = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return rating > 0; });

    ASSERT_EQUAL_HINT(even_id_documents.size(), 2, "Count of documents with even id should be 2"s);
    ASSERT_EQUAL_HINT(rating_more_1_documents.size(), 4, "Count of documents with rating > 0 should be 4"s);
}

// Тест проверяет поиск документов с заданным статусом
void TestFilteringByStatus() {
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3, doc_id_4 = 5, doc_id_5 = 6;
    const string content_1 = "animal fox on the table"s;
    const string content_2 = "pig in the city"s;
    const string content_3 = "cat in the city"s;
    const string content_4 = "animal dog in town"s;
    const string content_5 = "animal in town"s;
    const vector<int> ratings1 = { 1, 2, 3 };   //2
    const vector<int> ratings2 = { 5, 3, 1 };   //3
    const vector<int> ratings3 = { 0, 1 };   //0
    const vector<int> ratings4 = { 0, 2, 10, 4 };   //4
    const vector<int> ratings5 = { 1, 0, 6, 0 };    //1
    const string query = "animal in the city"s;

    SearchServer server;

    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings2);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings3);
    server.AddDocument(doc_id_4, content_4, DocumentStatus::IRRELEVANT, ratings4);
    server.AddDocument(doc_id_5, content_5, DocumentStatus::ACTUAL, ratings5);

    const auto actual_documents = server.FindTopDocuments(query);
    const auto banned_documents = server.FindTopDocuments(query, DocumentStatus::BANNED);
    const auto irr_documents = server.FindTopDocuments(query, DocumentStatus::IRRELEVANT);
    const auto removed_document = server.FindTopDocuments(query, DocumentStatus::REMOVED);

    ASSERT_EQUAL(actual_documents.size(), 3);
    ASSERT_EQUAL(banned_documents.size(), 1);
    ASSERT_EQUAL(irr_documents.size(), 1);
    ASSERT_EQUAL(removed_document.size(), 0);
}

// Тест проверят корректное вычисление релевантности
void TestDocumentRelevance() {
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3, doc_id_4 = 42;
    const string content_1 = "animal fox on the table"s;
    const string content_2 = "pig in the city"s;
    const string content_3 = "cat in the city"s;
    const string content_4 = "animal dog in town"s;
    const vector<int> ratings = { 1, 2, 3 };
    const string query = "animal in the city"s;

    SearchServer server;

    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_4, content_4, DocumentStatus::ACTUAL, ratings);

    const auto found_documents = server.FindTopDocuments(query);

    ASSERT(abs(found_documents[0].relevance - 0.317128) < 1e-6);
    ASSERT(abs(found_documents[1].relevance - 0.317128) < 1e-6);
    ASSERT(abs(found_documents[2].relevance - 0.245207) < 1e-6);
    ASSERT(abs(found_documents[3].relevance - 0.196166) < 1e-6);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddedDocumentIsSearchByQuery);
    RUN_TEST(TestNoMinusWordsDocumentsInQuery);
    RUN_TEST(TestMatchedDocuments);
    RUN_TEST(TestDocumentsSorting);
    RUN_TEST(TestDocumentRating);
    RUN_TEST(TestFilteringByPredicate);
    RUN_TEST(TestFilteringByStatus);
    RUN_TEST(TestDocumentRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------


int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;


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