#pragma once
#include "search_server.h"

void AddDocument(SearchServer& search_server, int document_id, const string& document,
	DocumentStatus status, const vector<int>& ratings);

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status);