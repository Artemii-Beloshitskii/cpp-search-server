#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)) {}

SearchServer::SearchServer(std::string_view stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, std::string_view document,
	DocumentStatus status, const std::vector<int>& ratings)
{
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw std::invalid_argument("Invalid document_id"s);
	}
	
	const auto words = SplitIntoWordsNoStop(document);
	
	const double inv_word_count = 1.0 / words.size();
	for (std::string_view word : words) {
		auto extra_word = storage.insert(std::string(word));

		word_to_document_freqs_[*extra_word.first][document_id] += inv_word_count;
		word_frequencies_[document_id][*extra_word.first] += inv_word_count;
	}
	
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });

	document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const
{
	return FindTopDocuments(
		raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const
{
	return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
	return documents_.size();
}

typename std::set<int>::const_iterator SearchServer::begin() const
{
	return SearchServer::document_ids_.begin();
}

typename std::set<int>::const_iterator SearchServer::end() const
{
	return SearchServer::document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	if (word_frequencies_.count(document_id) != 0) {
		return word_frequencies_.at(document_id);
	}
	else {
		static const std::map<std::string_view, double> empty;
		return empty;
	}
}

void SearchServer::RemoveDocument(int document_id)
{
	if (documents_.count(document_id) == 0) {
		return;
	}

	for (auto& word_to_delete : word_frequencies_.at(document_id)) {
		auto itr = word_to_document_freqs_.find(word_to_delete.first);
		itr->second.erase(document_id);
	}

	document_ids_.erase(document_id);
	documents_.erase(document_id);
	word_frequencies_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& p_p, int document_id) {
	if (documents_.count(document_id) == 0) {
		return;
	}

	const auto& words_to_delete = word_frequencies_.at(document_id);
	std::vector<std::string_view> words_to_delete_freqs(words_to_delete.size());

	std::transform(p_p, words_to_delete.begin(), words_to_delete.end(), words_to_delete_freqs.begin(),
		[](auto& ptr) { return ptr.first; });

	std::for_each(std::execution::par, words_to_delete_freqs.begin(), words_to_delete_freqs.end(),
		[this, &document_id](auto& word) { word_to_document_freqs_.find(word)->second.erase(document_id); });

	document_ids_.erase(document_id);
	documents_.erase(document_id);
	word_frequencies_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& s_p, int document_id) {
	RemoveDocument(document_id);
}

matched_documents SearchServer::MatchDocument(std::string_view raw_query, int document_id) const
{
	if (documents_.count(document_id) == 0) {
		throw std::out_of_range("Nonexistent document id");
	}
	if (!IsValidWord(raw_query)) {
		throw std::invalid_argument("Invalid raw query");
	}

	const auto query = ParseQuery(raw_query);

	std::vector<std::string_view> matched_words;

	for (std::string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.find(word)->second.count(document_id)) {
			return { {}, documents_.at(document_id).status };;
		}
	}

	for (std::string_view word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.find(word)->second.count(document_id)) {
			matched_words.push_back(word);
		}
	}

	return { matched_words, documents_.at(document_id).status };
}

matched_documents SearchServer::MatchDocument(const std::execution::parallel_policy& p_p,
	std::string_view raw_query, int document_id) const {
	if (documents_.count(document_id) == 0) {
		throw std::out_of_range("Nonexistent document id");
	}
	if (!IsValidWord(raw_query)) {
		throw std::invalid_argument("Invalid raw query");
	}

	const auto& result = ParseQuery(raw_query, false);

	std::vector<std::string_view> matched_words(result.plus_words.size());

	if (std::any_of(std::execution::par, result.minus_words.begin(), result.minus_words.end(), [this, &document_id](std::string_view word)
		{ return word_to_document_freqs_.find(word)->second.count(document_id) != 0; })) {
		return { {}, documents_.at(document_id).status };;
	}

	auto last_ptr = std::copy_if(std::execution::par, result.plus_words.begin(), result.plus_words.end(), matched_words.begin(),
		[this, &document_id](std::string_view word) { return word_to_document_freqs_.find(word)->second.count(document_id) != 0; });

	std::sort(std::execution::par, matched_words.begin(), last_ptr);
	last_ptr = std::unique(std::execution::par, matched_words.begin(), last_ptr);
	matched_words.erase(last_ptr, matched_words.end());

	return { matched_words, documents_.at(document_id).status };
}

matched_documents SearchServer::MatchDocument(const std::execution::sequenced_policy& s_p, std::string_view raw_query, int document_id) const
{
	return MatchDocument(raw_query, document_id);
}

bool SearchServer::IsStopWord(std::string_view word) const
{
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
	return std::none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const
{
	std::vector<std::string_view> words;
	for (std::string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = 0;
	for (const int rating : ratings) {
		rating_sum += rating;
	}
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{
	if (text.empty()) {
		throw std::invalid_argument("Query word is empty"s);
	}
	std::string_view word = text;
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw std::invalid_argument("Query word "s + std::string(text) + " is invalid");
	}

	return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool seq) const
{
	Query result;

	for (std::string_view word : SplitIntoWords(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.push_back(query_word.data);
			}
			else {
				result.plus_words.push_back(query_word.data);
			}
		}
	}

	if (seq) {
		auto& minus = result.minus_words;
		auto& plus = result.plus_words;

		auto last_ptr = std::unique(minus.begin(), minus.end());
		minus.erase(last_ptr, minus.end());
		sort(minus.begin(), minus.end());
		last_ptr = std::unique(minus.begin(), minus.end());
		minus.erase(last_ptr, minus.end());

		last_ptr = std::unique(plus.begin(), plus.end());
		plus.erase(last_ptr, plus.end());
		sort(plus.begin(), plus.end());
		last_ptr = std::unique(plus.begin(), plus.end());
		plus.erase(last_ptr, plus.end());
	}

	return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.find(word)->second.size());
}