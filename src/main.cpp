// SPDX-License-Identifier: MIT
#include "options.h"
////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>
#include <md4c-html.h>
#include <tidy.h>
#include <tidybuffio.h>
#include <tinyxml2.h>
#include <list>
#include <map>
#include <signal.h>

bool parse_framework(
    const std::string &path, const std::list<tinyxml2::XMLDocument> &sections,
    const std::map<std::string, std::string> &headings
);

bool parse_markdown(
    const std::string &path, std::list<tinyxml2::XMLDocument> &sections,
    std::function<std::string(const tinyxml2::XMLElement &heading)> callback
);

int main(int argc, char **argv) {
    OPTIONS options("MarkDown Monolith Assembler", "1.0", "Erich Erstu");

    bool fail{
        !options.deserialize(
            argc, argv,
            [&](const char *txt){
                std::cerr << txt << "\n";
            }
        )
    };

    if (fail) return EXIT_FAILURE;

    if (options.flags.exit) {
        return EXIT_SUCCESS;
    }

    int next_id = 1;
    std::list<tinyxml2::XMLDocument> sections;
    std::map<std::string, std::string> headings;

    fail = (
        !parse_markdown(
            options.file, sections,
            [&next_id, &headings](const tinyxml2::XMLElement &heading) {
                std::string id{"anchor-"};
                id.append(std::to_string(next_id++));

                const char *title = heading.GetText();

                if (title) {
                    headings.emplace(id, std::string(title));
                }

                return id;
            }
        )
    );

    if (fail) return EXIT_FAILURE;

    if (!parse_framework(options.framework, sections, headings)) {
        return EXIT_FAILURE;
    }

    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

tinyxml2::XMLElement *find_if(
    const tinyxml2::XMLElement &root,
    std::function<bool(const tinyxml2::XMLElement &, int)> fun
);

bool parse_markdown(
    const std::string &path, std::list<tinyxml2::XMLDocument> &sections,
    std::function<std::string(const tinyxml2::XMLElement &heading)> callback
) {
    std::string markdown;

    if (path.empty()) {
        std::ostringstream std_input;
        std_input << std::cin.rdbuf();
        markdown.assign(std_input.str());
    }
    else {
        std::ifstream input(path, std::ios::binary);

        if (!input) {
            std::cerr << path << ": " << strerror(errno) << "\n";
            return false;
        }

        std::stringstream sstr;
        input >> sstr.rdbuf();
        markdown.assign(sstr.str());
    }

    if (markdown.size() > std::numeric_limits<MD_SIZE>::max()) {
        std::cerr << path << ": file size limit exceeded\n";
        return false;
    }

    std::string xhtml;

    bool fail = md_html(
        markdown.c_str(), MD_SIZE(markdown.size()),
        [](const MD_CHAR *str, MD_SIZE len, void *userdata) {
            std::string *dest = static_cast<std::string *>(userdata);
            dest->append(str, len);
        },
        &xhtml, MD_DIALECT_GITHUB, MD_HTML_FLAG_XHTML
    );

    if (fail) {
        std::cerr << path << ": unable to convert into HTML\n";
        return false;
    }

    tinyxml2::XMLDocument doc;
    doc.Parse(xhtml.c_str(), xhtml.size());
    tinyxml2::XMLElement *parent = doc.RootElement();

    if (!parent) {
        std::cerr << path << ": unacceptable content\n";
        return false;
    }

    const tinyxml2::XMLElement *first_heading = find_if(
        *parent,
        [](const tinyxml2::XMLElement &el, int depth) {
            const char *name = el.Name();

            if (name) {
                if (!strcasecmp("h1", name)) {
                    return true;
                }
            }

            return false;
        }
    );

    const tinyxml2::XMLElement *sibling = first_heading;

    for (; sibling; sibling = sibling->NextSiblingElement()) {
        tinyxml2::XMLElement *root = nullptr;
        const char *name = sibling->Name();

        if (!name) {
            continue;
        }

        bool new_tab = !strcasecmp("h1", name);

        if (new_tab
        ||  !strcasecmp("h2", name)
        ||  !strcasecmp("h3", name)
        ||  !strcasecmp("h4", name)
        ||  !strcasecmp("h5", name)
        ||  !strcasecmp("h6", name)) {
            tinyxml2::XMLNode *node = nullptr;
            tinyxml2::XMLElement *elem = nullptr;

            if (new_tab) {
                sections.emplace_back();

                node = sections.back().InsertFirstChild(
                    sections.back().NewElement("article")
                );

                elem = node ? node->ToElement() : nullptr;

                if (elem) {
                    elem->SetAttribute("class", "tab");
                }
            }
            else {
                elem = sections.back().RootElement();
            }

            if (elem) {
                node = elem->InsertEndChild(
                    sections.back().NewElement("a")
                );

                elem = node ? node->ToElement() : nullptr;

                if (elem) {
                    std::string id(callback(*sibling));

                    elem->SetAttribute("id", id.c_str());
                    elem->SetAttribute(
                        "href", std::string("#").append(id).c_str()
                    );

                    root = elem;
                }
                else raise(SIGSEGV);
            }
            else raise(SIGSEGV);
        }

        if (sections.empty()) continue;

        if (!root) {
            root = sections.back().RootElement();
        }

        tinyxml2::XMLNode *node = sibling->DeepClone(&sections.back());

        if (node) {
            root->InsertEndChild(node);
        }
    }

    return true;
}

void assemble_framework_body(
    std::string &body_html, const std::list<tinyxml2::XMLDocument> &sections,
    const std::map<std::string, std::string> &headings
) {
    static constexpr const char *smallest_valid_html5{
        "<!DOCTYPE html><title>x</title>"
    };

    tinyxml2::XMLDocument doc;

    {
        // Let's convert HTML to XML.

        std::string html5(smallest_valid_html5);
        html5.append(body_html);

        TidyBuffer tidy_buffer{};
        TidyDoc tdoc_xml = tidyCreate();

        tidyOptSetBool(tdoc_xml, TidyHideComments, yes);
        tidyOptSetBool(tdoc_xml, TidyMergeSpans, no);
        tidyOptSetBool(tdoc_xml, TidyDropEmptyElems, no);
        tidyOptSetBool(tdoc_xml, TidyDropEmptyParas, no);
        tidyOptSetBool(tdoc_xml, TidyXmlOut, yes);
        tidyParseString(tdoc_xml, html5.c_str());

        TidyNode body_node = tidyGetBody(tdoc_xml);
        tidyNodeGetText(tdoc_xml, body_node, &tidy_buffer);

        doc.Parse((const char *) tidy_buffer.bp, tidy_buffer.size);

        tidyRelease(tdoc_xml);
        tidyBufFree(&tidy_buffer);
    }

    tinyxml2::XMLElement *parent = doc.RootElement();

    tinyxml2::XMLElement *content = find_if(
        *parent,
        [](const tinyxml2::XMLElement &el, int) {
            const char *name = el.Name();

            if (name && strcasecmp("div", name)) {
                return false;
            }

            const char *id = "";

            if (el.QueryStringAttribute("id", &id) != tinyxml2::XML_SUCCESS) {
                return false;
            }

            if (id && !strcasecmp("MDMA-CONTENT", id)) {
                return true;
            }

            return false;
        }
    );

    if (content) {
        content->DeleteChildren();

        for (const tinyxml2::XMLDocument &section : sections) {
            const tinyxml2::XMLElement *root = section.RootElement();
            tinyxml2::XMLNode *node = root->DeepClone(&doc);

            if (node) {
                content->InsertEndChild(node);
            }
        }
    }

    tinyxml2::XMLElement *agenda = find_if(
        *parent,
        [](const tinyxml2::XMLElement &el, int) {
            const char *name = el.Name();

            if (name && strcasecmp("div", name)) {
                return false;
            }

            const char *id = "";

            if (el.QueryStringAttribute("id", &id) != tinyxml2::XML_SUCCESS) {
                return false;
            }

            if (id && !strcasecmp("MDMA-AGENDA", id)) {
                return true;
            }

            return false;
        }
    );

    if (agenda) {
        agenda->DeleteChildren();

        for (const auto &p : headings) {
            tinyxml2::XMLNode *node = agenda->InsertEndChild(
                doc.NewElement("a")
            );

            tinyxml2::XMLElement *elem = node ? node->ToElement() : nullptr;

            if (elem) {
                elem->SetAttribute(
                    "href", std::string("#").append(p.first).c_str()
                );

                elem->InsertNewText(p.second.c_str());
            }
            else raise(SIGSEGV);
        }
    }

    tinyxml2::XMLPrinter printer(nullptr, true);
    doc.Print(&printer);

    body_html.assign(printer.CStr());

    {
        // Let's convert XML to HTML.

        std::string html5(smallest_valid_html5);
        html5.append(body_html);

        TidyBuffer tidy_buffer{};
        TidyDoc tdoc = tidyCreate();

        tidyOptSetBool(tdoc, TidyHideComments, yes);
        tidyOptSetBool(tdoc, TidyMergeSpans, no);
        tidyOptSetBool(tdoc, TidyDropEmptyElems, no);
        tidyOptSetBool(tdoc, TidyDropEmptyParas, no);
        tidyParseString( tdoc, html5.c_str() );

        TidyNode body = tidyGetBody(tdoc);

        tidyNodeGetText(tdoc, body, &tidy_buffer);
        body_html.assign(
            (const char *) tidy_buffer.bp, tidy_buffer.size
        );

        tidyRelease(tdoc);
        tidyBufFree(&tidy_buffer);
    }
}

TidyNode find_if(TidyNode root, std::function<bool(const TidyNode &, int)> fun);

bool parse_framework(
    const std::string &path, const std::list<tinyxml2::XMLDocument> &sections,
    const std::map<std::string, std::string> &headings
) {
    auto default_framework{std::to_array<unsigned char>({ MDMA_FRAMEWORK })};
    std::string framework;

    if (path.empty()) {
        framework.assign(
            (const char *) default_framework.data(), default_framework.size()
        );
    }
    else {
        std::ifstream input(path, std::ios::binary);

        if (!input) {
            std::cerr << path << ": " << strerror(errno) << "\n";
            return false;
        }

        std::stringstream sstr;
        input >> sstr.rdbuf();
        framework.assign(sstr.str());
    }

    TidyBuffer tidy_buffer{};
    TidyDoc tdoc = tidyCreate();

    tidyOptSetBool(tdoc, TidyHideComments, yes);
    tidyOptSetBool(tdoc, TidyMergeSpans, no);
    tidyOptSetBool(tdoc, TidyDropEmptyElems, no);
    tidyOptSetBool(tdoc, TidyDropEmptyParas, no);
    tidyOptSetBool(tdoc, TidyIndentContent, yes);
    tidyParseString( tdoc, framework.c_str() );
    framework.clear();

    do {
        TidyNode style = find_if(
            tidyGetHead(tdoc),
            [](const TidyNode &node, int) {
                TidyAttr attr = tidyAttrGetById(node, TidyAttr_CLASS);

                if (attr
                && !strcasecmp(tidyAttrValue(attr), "MDMA-REJECT")) {
                    return true;
                }

                return false;
            }
        );

        if (!style) break;

        tidyDiscardElement(tdoc, style);
    }
    while (true);

    std::string agenda_css;
    size_t heading_counter = 0;

    for (const auto &p : headings) {
        const std::string &anchor_id = p.first;
        ++heading_counter;

        agenda_css.append("#MDMA-CONTENT:has(#").append(anchor_id).append(
            ":target) ~ .menu a[href=\"#"
        ).append(anchor_id).append("\"]");

        if (heading_counter == headings.size()) {
            agenda_css.append(" {\n").append("    color: green;\n}\n");
        }
        else {
            agenda_css.append(",\n");
        }
    }

    std::map<int, std::string, std::greater<int>> end_tags;

    find_if(
        tidyGetRoot(tdoc),
        [&](const TidyNode &node, int depth) {
            while (!end_tags.empty()) {
                int d = end_tags.begin()->first;

                if (depth <= d) {
                    framework.append(end_tags.begin()->second);
                    end_tags.erase(d);
                }
                else break;
            }

            switch (depth) {
                case 1: {
                    if (tidyNodeGetId(node) != TidyTag_HTML) {
                        tidyNodeGetText(tdoc, node, &tidy_buffer);
                        framework.append(
                            (const char *) tidy_buffer.bp, tidy_buffer.size
                        );
                        tidyBufClear(&tidy_buffer);
                    }
                    else {
                        framework.append("<html>\n");
                        end_tags[depth].append("</html>\n");
                    }

                    break;
                }
                case 2: {
                    TidyNode parent = tidyGetParent(node);

                    if (tidyNodeGetId(parent) != TidyTag_HTML) {
                        break;
                    }

                    if (tidyNodeGetId(node) == TidyTag_HEAD) {
                        framework.append("<head>\n");

                        if (!headings.empty()) {
                            end_tags[depth].append("<style>\n").append(
                                agenda_css
                            ).append("</style>\n");
                        }

                        end_tags[depth].append("</head>\n");
                    }
                    else if (tidyNodeGetId(node) == TidyTag_BODY) {
                        tidyNodeGetText(tdoc, node, &tidy_buffer);
                        std::string body_html(
                            (const char *) tidy_buffer.bp, tidy_buffer.size
                        );
                        tidyBufClear(&tidy_buffer);

                        assemble_framework_body(body_html, sections, headings);

                        framework.append(body_html);
                    }
                    else {
                        tidyNodeGetText(tdoc, node, &tidy_buffer);
                        framework.append(
                            (const char *) tidy_buffer.bp, tidy_buffer.size
                        );
                        tidyBufClear(&tidy_buffer);
                    }

                    break;
                }
                case 3: {
                    TidyNode parent = tidyGetParent(node);

                    if (tidyNodeGetId(parent) != TidyTag_HEAD) {
                        break;
                    }

                    tidyNodeGetText(tdoc, node, &tidy_buffer);
                    framework.append(
                        (const char *) tidy_buffer.bp, tidy_buffer.size
                    );
                    tidyBufClear(&tidy_buffer);

                    break;
                }
                default: break;
            }

            return false;
        }
    );

    for (const auto &p : end_tags) {
        framework.append(p.second);
    }

    tidyParseString(tdoc, framework.c_str());
    tidyCleanAndRepair(tdoc);
    tidySaveBuffer(tdoc, &tidy_buffer);

    framework.assign((const char *) tidy_buffer.bp, tidy_buffer.size);

    tidyRelease(tdoc);
    tidyBufFree(&tidy_buffer);

    std::cout << framework << "\n";

    return true;
}

tinyxml2::XMLElement *find_if(
    const tinyxml2::XMLElement &root,
    std::function<bool(const tinyxml2::XMLElement &, int)> fun
) {
    int depth = 0;

    const tinyxml2::XMLElement *parent = &root;

    while (parent) {
        if (fun(*parent, depth)) {
            return const_cast<tinyxml2::XMLElement *>(parent);
        }

        const tinyxml2::XMLElement *child = parent->FirstChildElement();

        if (child) {
            parent = child;
            ++depth;
            continue;
        }

        const tinyxml2::XMLElement *sibling = parent->NextSiblingElement();

        if (sibling) {
            parent = sibling;
            continue;
        }

        do {
            const tinyxml2::XMLNode *parent_node = parent->Parent();

            parent = parent_node ? parent_node->ToElement() : nullptr;

            if (!parent) break;

            sibling = parent->NextSiblingElement();

            --depth;
        }
        while (!sibling);

        parent = sibling;
    }

    return nullptr;
}

TidyNode find_if(
    const TidyNode root, std::function<bool(const TidyNode &, int)> fun
) {
    int depth = 0;

    TidyNode parent = root;

    while (parent) {
        if (fun(parent, depth)) {
            return const_cast<TidyNode>(parent);;
        }

        const TidyNode child = tidyGetChild(parent);

        if (child) {
            parent = child;
            ++depth;
            continue;
        }

        TidyNode sibling = tidyGetNext(parent);

        if (sibling) {
            parent = sibling;
            continue;
        }

        do {
            parent = tidyGetParent(parent);

            if (!parent) break;

            sibling = tidyGetNext(parent);

            --depth;
        }
        while (!sibling);

        parent = sibling;
    }

    return {};
}
