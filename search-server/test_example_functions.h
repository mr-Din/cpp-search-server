#pragma once
#include "search_server.h"

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
	DocumentStatus status, const std::vector<int>& ratings);

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);