#include "reader.h"
#include "searcher.h"

#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPanel.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include <algorithm>
#include <libgen.h>
#include <iostream>
#include <sstream>
#include <memory>

namespace {

std::string MakeWikiUrl(const std::string &title) {
    // I should probably do some escaping or whatever...
    return "https://en.wikipedia.org/wiki/" + title;
}

std::unique_ptr<Wt::WAnchor> MakeWikiAnchor(const std::string &title) {
    Wt::WLink link(MakeWikiUrl(title));
    link.setTarget(Wt::LinkTarget::NewWindow);
    auto anchor = std::make_unique<Wt::WAnchor>(link, title);
    anchor->setTextFormat(Wt::TextFormat::Plain);
    return anchor;
}

// Formats a number with a seperator after each group of three.
// FormatNumber(42) == "42"
// FormatNumber(1234567890) == "1,234,567,890"
std::string FormatNumber(int64_t i, char sep = ',') {
    if (i == 0) return "0";
    std::ostringstream oss;
    uint64_t u = i < 0 ? -i : i;
    for (int digits = 0; u > 0; ++digits, u /= 10) {
        if (digits == 3) {
            digits = 0;
            oss << sep;
        }
        oss << char('0' + u%10);
    }
    if (i < 0) oss << '-';
    std::string s = oss.str();
    std::reverse(s.begin(), s.end());
    return s;
}

class SearchApplication : public Wt::WApplication
{
public:
    SearchApplication(const Wt::WEnvironment& env, const char *graph_filename, Reader &reader);

private:
    void RandomizeStart();
    void RandomizeFinish();
    void Search();
    void SurpriseMe();
    void ShowError(const std::string &message);

    Reader &reader;
    Wt::WLineEdit *startEdit = nullptr;
    Wt::WLineEdit *finishEdit = nullptr;
    Wt::WTemplate *resultTemplate = nullptr;
    Wt::WTemplate *notFoundTemplate = nullptr;
    Wt::WTemplate *errorTemplate = nullptr;
    Wt::WTemplate *statsTemplate = nullptr;
    Wt::WTemplate *surpriseButton = nullptr;
};

SearchApplication::SearchApplication(const Wt::WEnvironment& env, const char *graph_filename, Reader &reader)
    : Wt::WApplication(env), reader(reader) {
    setTitle("Wikipedia Graph Search");

    root()->addWidget(std::make_unique<Wt::WText>("<h1>Wikipedia Shortest Path Search</h1>"));

    // Search form
    auto formTemplate = std::make_unique<Wt::WTemplate>(R"(
<div class="input-form">
<p><label><b>Origin page:</b><br/>${start-edit}</label>${random-start-button}</p>
<p><label><b>Destination page:</b><br/>${finish-edit}</label>${random-finish-button}</p>
<p>${search-button} ${surprise-me-button} </p>
</div>)");
    startEdit = formTemplate->bindWidget("start-edit", std::make_unique<Wt::WLineEdit>());
    startEdit->setTextSize(40);
    formTemplate->bindWidget("random-start-button", std::make_unique<Wt::WPushButton>("🎲"))
            ->clicked().connect(this, &SearchApplication::RandomizeStart);
    finishEdit = formTemplate->bindWidget("finish-edit", std::make_unique<Wt::WLineEdit>());
    finishEdit->setTextSize(40);
    formTemplate->bindWidget("random-finish-button", std::make_unique<Wt::WPushButton>("🎲"))
            ->clicked().connect(this, &SearchApplication::RandomizeFinish);
    formTemplate->bindWidget("search-button",  std::make_unique<Wt::WPushButton>("Search!"))
            ->clicked().connect(this, &SearchApplication::Search);
    formTemplate->bindWidget("surprise-me-button",  std::make_unique<Wt::WPushButton>("Surprise Me!"))
            ->clicked().connect(this, &SearchApplication::SurpriseMe);
    root()->addWidget(std::move(formTemplate));

    resultTemplate = root()->addWidget(std::make_unique<Wt::WTemplate>(R"(
<h3>Shortest path found!</h3>
<p>Shortest path from ${start} to ${finish}:</p>
${path})"));
    resultTemplate->hide();

    notFoundTemplate = root()->addWidget(std::make_unique<Wt::WTemplate>(R"(
<h3>Path not found!</h3>
<p>There is no path from ${start} to ${finish}! (This often happens when the destination is a redirect page without incoming links.)</p>)"));
    notFoundTemplate->hide();

    errorTemplate = root()->addWidget(std::make_unique<Wt::WTemplate>(R"(<h3>Error!</h3><p>⚠ ${message}</p>)"));
    errorTemplate->hide();

    auto statsHeaderTemplate = std::make_unique<Wt::WTemplate>(R"(
<h3>Stats for nerds</h3>
<p>Serving <code>${filename}</code>, which contains ${size} pages.</p>)");
    statsHeaderTemplate->bindString("filename", graph_filename, Wt::TextFormat::Plain);
    statsHeaderTemplate->bindString("size", FormatNumber(reader.Graph().Size()), Wt::TextFormat::Plain);
    root()->addWidget(std::move(statsHeaderTemplate));

    statsTemplate = root()->addWidget(std::make_unique<Wt::WTemplate>(R"(
<p>Search took <b>${time-taken-ms}</b> milliseconds.<br/>
<b>${vertices-reached}</b> vertices reached.<br/>
<b>${vertices-expanded}</b> vertices expanded.<br/>
<b>${edges-expanded}</b> edges expanded.</p>)"));
    statsTemplate->hide();

    root()->addWidget(std::make_unique<Wt::WText>(R"(<p>Source code: <a href="https://github.com/maksverver/WikipediaGraphSearch" target="_blank">https://github.com/maksverver/WikipediaGraphSearch</a>.</p>)"));

    RandomizeStart();
    RandomizeFinish();
}

void SearchApplication::ShowError(const std::string &message) {
    errorTemplate->bindString("message", message, Wt::TextFormat::Plain);
    errorTemplate->show();
}

void SearchApplication::RandomizeStart() {
    startEdit->setText(reader.PageTitle(reader.RandomPageId()));
}

void SearchApplication::RandomizeFinish() {
    finishEdit->setText(reader.PageTitle(reader.RandomPageId()));
}

void SearchApplication::Search() {
    resultTemplate->hide();
    notFoundTemplate->hide();
    errorTemplate->hide();
    statsTemplate->hide();

    std::string startTitle = startEdit->text().toUTF8();
    if (startTitle.empty()) {
        ShowError("Origin cannot be empty!");
        return;
    }
    std::optional<MetadataReader::Page> startPage = reader.Metadata().GetPageByTitle(startTitle);
    if (!startPage) {
        ShowError("Origin page not found! (Note: page titles are case-sensitive!)");
        return;
    }

    std::string finishTitle = finishEdit->text().toUTF8();
    if (finishTitle.empty()) {
        ShowError("Destination cannot be empty!");
        return;
    }
    std::optional<MetadataReader::Page> finishPage = reader.Metadata().GetPageByTitle(finishTitle);
    if (!finishPage) {
        ShowError("Destination page not found! (Note: page titles are case-sensitive!)");
        return;
    }

    SearchStats stats;
    std::vector<index_t> path = FindShortestPath(reader.Graph(), startPage->id, finishPage->id, &stats);

    // Display stats.
    statsTemplate->bindString("vertices-reached", FormatNumber(stats.vertices_reached), Wt::TextFormat::Plain);
    statsTemplate->bindString("vertices-expanded", FormatNumber(stats.vertices_expanded), Wt::TextFormat::Plain);
    statsTemplate->bindString("edges-expanded", FormatNumber(stats.edges_expanded), Wt::TextFormat::Plain);
    statsTemplate->bindString("time-taken-ms", FormatNumber(stats.time_taken_ms), Wt::TextFormat::Plain);
    statsTemplate->show();

    if (path.empty()) {
        notFoundTemplate->bindWidget("start", MakeWikiAnchor(startPage->title));
        notFoundTemplate->bindWidget("finish", MakeWikiAnchor(finishPage->title));
        notFoundTemplate->show();
    } else {
        resultTemplate->bindWidget("start", MakeWikiAnchor(startPage->title));
        resultTemplate->bindWidget("finish", MakeWikiAnchor(finishPage->title));
        auto pathContainer = resultTemplate->bindWidget("path", std::make_unique<Wt::WContainerWidget>());
        pathContainer->setList(/* list= */ true, /* ordered= */ true);
        index_t prev_i = 0;
        for (index_t i : path) {
            std::string title = reader.PageTitle(i);
            std::string text = prev_i != 0 ? reader.LinkText(prev_i, i) : title;
            Wt::WTemplate *stepTemplate = nullptr;
            if (title == text) {
                // Note: I'm using a template widget here, because adding a WAnchor widget directly
                // doesn't seem to work (the widget simply doesn't appear in the list!)
                stepTemplate = pathContainer->addWidget(std::make_unique<Wt::WTemplate>("${title}"));
            } else {
                stepTemplate = pathContainer->addWidget(std::make_unique<Wt::WTemplate>("${title} (displayed as <em>${text}</em>)"));
                stepTemplate->bindString("text", text, Wt::TextFormat::Plain);
            }
            stepTemplate->bindWidget("title", MakeWikiAnchor(title));
            prev_i = i;
        }
        resultTemplate->show();
    }
}

void SearchApplication::SurpriseMe() {
    RandomizeStart();
    RandomizeFinish();
    Search();
}

}  // namespace

// Web frontend for Wikipedia graph search. Example run:
// ./websearch wiki.graph --docroot /usr/share/Wt --http-address 127.0.0.1 --http-port 8001
int main(int argc, char **argv) {
    if (argc < 2 || argv[1][0] == '-') {
        std::cout << "Usage: " << argv[0] << " <wiki.graph> ... (--help shows Web Toolkit options)\n";
        return EXIT_FAILURE;
    }

    auto ExtractArg = [&argc, &argv](int index) -> char* {
        if (index < 0 || index >= argc) return nullptr;
        char *result = argv[index];
        --argc;
        for (int i = index; i < argc; ++i) argv[i] = argv[i + 1];
        argv[argc] = nullptr;
        return result;
    };

    char *graph_filename = ExtractArg(1);
    std::unique_ptr<Reader> reader = Reader::Open(graph_filename);
    if (reader == nullptr) {
        return EXIT_FAILURE;
    }

    return Wt::WRun(argc, argv, [&reader, graph_filename](const Wt::WEnvironment& env) {
        return std::make_unique<SearchApplication>(env, basename(graph_filename), *reader);
    });
}
