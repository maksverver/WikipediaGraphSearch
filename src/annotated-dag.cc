#include "wikipath/annotated-dag.h"

#include "wikipath/reader.h"

#include <unordered_map>

namespace wikipath {

std::string_view AnnotatedLink::Text() const {
    if (reader_or_text.index() == 0) {
        reader_or_text = std::get<0>(reader_or_text)->
                LinkText(src->Id(), dst->Id());
    }
    return std::get<1>(reader_or_text);
}

std::string AnnotatedLink::ForwardRef() const {
    return ForwardLinkRef(dst->Id(), dst->Title(), Text());
}

std::string AnnotatedLink::BackwardRef() const {
    return BackwardLinkRef(src->Id(), src->Title(), dst->Title(), Text());
}

std::string_view AnnotatedPage::Title() const {
    if (reader_or_title.index() == 0) {
        reader_or_title = std::get<0>(reader_or_title)->PageTitle(id);
    }
    return std::get<1>(reader_or_title);
}

int64_t AnnotatedPage::CalculatePathCount(const AnnotatedPage *page, const AnnotatedPage *finish) {
	if (page == finish) return 1;
	int64_t res = 0;
	for (const auto &link : page->Adj()) res += link.Dst()->PathCount(finish);
	return res;
}

AnnotatedDag::AnnotatedDag(
		const Reader *reader, index_t start_id, index_t finish_id,
		const std::vector<std::pair<index_t, index_t>> &edge_list)
            : reader(reader) {

    std::unordered_map<index_t, size_t> page_index_by_id;
    auto reserve_page_index = [&](index_t v) {
        auto [it, inserted] = page_index_by_id.insert({v, 0});
        if (inserted) {
            it->second = pages.size();
            pages.push_back(AnnotatedPage(v, reader));
        }
        return it->second;
    };

    // Pass 1: reserve an index for each unique page id. This is done up front,
    // so we can use pointers to pages in pass 2, without having to worry about
    // pointers being invalidated when the `pages` vector is resized.
    size_t start_index = reserve_page_index(start_id);
    size_t finish_index = reserve_page_index(finish_id);
    for (auto [v, w] : edge_list) {
        reserve_page_index(v);
        reserve_page_index(w);
    }

    // Pass 2: create links between pages.
    start = &pages[start_index];
    finish = &pages[finish_index];
    for (auto [v, w] : edge_list) {
        size_t i = page_index_by_id.at(v);
        size_t j = page_index_by_id.at(w);
        pages[i].AddLink(&pages[j], reader);
    }
}

bool AnnotatedDag::EnumeratePaths(const EnumeratePathsCallback &callback, int64_t offset) const {
    return EnumeratePathsContext{
        .finish = finish,
        .callback = &callback,
        .offset = offset,
        .links = {},
    }.EnumeratePaths(start);
}

int64_t AnnotatedDag::CountPaths() {
    return start->PathCount(finish);
}

bool AnnotatedDag::EnumeratePathsContext::EnumeratePaths(const AnnotatedPage *page) {
    if (page == finish) {
        return offset == 0 ? (*callback)(links) : true;
    }
    for (const AnnotatedLink &link : page->Adj()) {
        links.push_back(&link);
        int64_t n;
        if (offset > 0 && (n = link.Dst()->PathCount(finish)) <= offset) {
            offset -= n;
        } else {
            if (!EnumeratePaths(link.Dst())) return false;
        }
        links.pop_back();
    }
    return true;
}

}  // namespace wikipath
