#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    
    vector<int> ids_for_remove;

    map<set<string>, int> words_id;

    for (const int document_id : search_server) {
        set<string> words_set;
        const auto& words_freqs = search_server.GetWordFrequencies(document_id);
        for (const auto& [word, _] : words_freqs) {
            words_set.insert(word);
        }
        if (words_id.count(words_set)==0) {
            words_id[words_set]= document_id;
        }
        else {
            ids_for_remove.push_back(document_id);
        }
    }

    for (int id : ids_for_remove) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id "s << id << endl;
    }

}