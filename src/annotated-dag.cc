#include "wikipath/annotated-dag.h"

#include "wikipath/reader.h"

#include <cassert>
#include <algorithm>
#include <locale>
#include <string_view>
#include <unordered_map>

namespace {

// Comparator that compares strings using the given locale.
struct LocaleStringLess {
    std::locale locale = std::locale();  // use global locale

    bool operator()(std::string_view a, std::string_view b) const {
        return std::use_facet<std::collate<char>>(locale).compare(
            a.data(), a.data() + a.size(), b.data(), b.data() + b.size()) < 0;
    }
};

// This causes the locale to be inherited from the environment.
static std::locale native_locale("");

}  // namespace

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
	for (const auto &link : page->links) res += link.Dst()->PathCount(finish);
	return res;
}

void AnnotatedPage::SortLinks(std::vector<AnnotatedLink> &links, LinkOrder order) {

    switch (order) {
        case LinkOrder::ID:
            std::ranges::sort(links, {},
                    [](const AnnotatedLink &link) { return link.Dst()->Id(); });
            break;

        case LinkOrder::TITLE:
            std::ranges::sort(links, LocaleStringLess{native_locale},
                    [](const AnnotatedLink &link) { return link.Dst()->Title(); });
            break;

        case LinkOrder::TEXT:
            std::ranges::sort(links, LocaleStringLess{native_locale},
                    [](const AnnotatedLink &link) { return link.Text(); });
            break;
    }
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

bool AnnotatedDag::EnumeratePaths(
        enumerate_paths_callback_t callback,
        int64_t offset,
        LinkOrder order) const {
    return EnumeratePathsContext{
        .finish = finish,
        .callback = std::move(callback),
        .offset = offset,
        .order = order,
        .links = {},
    }.EnumeratePaths(start);
}

int64_t AnnotatedDag::CountPaths() const {
    return start->PathCount(finish);
}

bool AnnotatedDag::EnumeratePathsContext::EnumeratePaths(const AnnotatedPage *page) {
    if (page == finish) {
        return offset == 0 ? callback(links) : true;
        return callback(links);
    }
    for (const AnnotatedLink &link : page->Links(order)) {
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

// This function moves up the stack to find a page in the DAG from which we can
// reach the finish after skipping `skip` paths.
//
// For example, if the DAG looks like this:
//
//
//         c              |
//       /  \             |
//      a     \           |
//    /  \      \         |
//  s      d --- f        |
//    \  /      /         |
//      b      /          |
//       \   /            |
//         e              |
//
//
// Then the first path creates state:
//
//   path  = {s->a, a->c, c->f}
//   stack = {nullptr, s->b, nullptr, a->d, nullptr}
//
// And if skip == 0, we compute:
//
//   path  = {s->a, a->d}
//   stack = {nullptr, s->b, nullptr}
//   page  = d
//
// But if skip == 1:
//
//   path = {s->b}
//   stack = {nullptr}
//   page = b
//
// etc.
//
// This function returns null if skip >= start.CountPaths(finish).
//
const AnnotatedPage *PathEnumerator::AdvanceToNextPage(int64_t &skip) {
    while (!stack.empty()) {
        const AnnotatedLink *link = stack.back();
        stack.pop_back();
        if (link == nullptr) {
            path.pop_back();
        } else {
            int64_t n;
            const AnnotatedPage *page = link->Dst();
            if (skip > 0 && (n = page->PathCount(finish)) <= skip) {
                skip -= n;
            } else {
                path.back() = link;
                return page;
            }
        }
    }
    return nullptr;
}

// Finds a path to the finish after skipping `skip` paths (phrased differently:
// finds the (skip + 1)'th path), or returns false if that path does not exist.
bool PathEnumerator::FindPathToFinish(const AnnotatedPage *page, int64_t skip) {
    assert(page != finish);
    while (page != finish) {
        auto links = page->Links(order);
        size_t i = 0;
        int64_t n;
        while (i < links.size() && skip > 0 && (n = links[i].Dst()->PathCount(finish)) <= skip) {
            skip -= n;
            ++i;
        }
        if (i < links.size()) {
            page = links[i].Dst();
            path.push_back(&links[i]);
            stack.push_back(nullptr);
            for (size_t j = links.size() - 1; j > i; --j) stack.push_back(&links[j]);
        } else {
            page = AdvanceToNextPage(skip);
            if (page == nullptr) return false;
        }
    }
    assert(skip == 0);
    return true;
}

}  // namespace wikipath
