#include "process_queries.h"
#include <numeric>
#include <execution>
#include <functional>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> res(queries.size());
    std::transform(std::execution::par,
        queries.begin(), queries.end(),
        res.begin(),
        [&search_server](std::string const& query) {return search_server.FindTopDocuments(query);});
    return res;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    auto queries_to_documents = ProcessQueries(search_server, queries);

    int s = 0;
    for (auto const& query_to_doc : queries_to_documents) {
        s += query_to_doc.size();
    }
    std::vector<Document> documents(s);

    int next_pos = 0;
    for (auto const& query_to_doc : queries_to_documents) {
        std::move(query_to_doc.begin(), query_to_doc.end(),
            (documents.begin()+ next_pos));
        next_pos += query_to_doc.size();
    }

    return documents;
}