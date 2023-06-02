// SPDX-License-Identifier: MIT
#ifndef MDMA_H_02_06_2023
#define MDMA_H_02_06_2023
#include <cstdlib>
#include <functional>
#include <string>
#include <cstdarg>
#include <map>
#include <tidy.h>
#include <tidybuffio.h>
#include <list>
#include <tinyxml2.h>
#include <md4c-html.h>
#include <signal.h>

class MDMA {
    public:
    static constexpr const char *CAPTION = "MarkDown Monolith Assembler";
    static constexpr const char *VERSION = "1.0";
    static constexpr const char *AUTHOR  = "Erich Erstu";

    MDMA() : cfg(
        {
            .minify = false
        }
    )
    , assembly_buffer("")
    , log_callback(nullptr) {}

    ~MDMA() {}

    struct cfg_type {
        bool minify:1;
    } cfg;

    void set_logger(const std::function<void(const char *)>& log_callback);

    const std::string *assemble(
        const char *htm, size_t htm_sz, const char *md, size_t md_sz
    );

    private:
    void bug(const char * =__builtin_FILE(), int =__builtin_LINE()) const;
    void log(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    void die(const char * =__builtin_FILE(), int =__builtin_LINE()) const;

    bool parse_framework(
        std::string &framework,
        std::function<void(const TidyNode &node)> callback
    );

    bool parse_markdown(
        const char *str, size_t len, std::list<tinyxml2::XMLDocument> &sections,
        std::function<int(const tinyxml2::XMLElement &heading)> callback
    );

    bool fill_framework(
        std::string &framework,
        const std::list<tinyxml2::XMLDocument> &sections,
        const std::map<int, std::pair<int, std::string>> &headings
    );

    int add_heading_and_get_parent_id(
        int id, int level, std::map<int, int> &level_to_id
    ) const;

    void setup(TidyDoc) const;

    TidyNode find_if(
        TidyNode root, std::function<bool(const TidyNode &, int)> fun
    ) const;

    tinyxml2::XMLNode *find_if(
        const tinyxml2::XMLNode &root,
        std::function<bool(const tinyxml2::XMLNode &, int)> fun
    ) const;

    void assemble_framework_body(
        std::string &body_html,
        const std::list<tinyxml2::XMLDocument> &sections,
        const std::map<int, std::pair<int, std::string>> &headings
    ) const;

    std::string assembly_buffer;
    std::function<void(const char *text)> log_callback;
};

void MDMA::set_logger (const std::function<void(const char *)>& log_cb) {
    log_callback = log_cb;
}

void MDMA::log(const char *fmt, ...) const {
    if (!log_callback) return;

    char stackbuf[256];
    char *bufptr = stackbuf;
    size_t bufsz = sizeof(stackbuf);

    for (size_t i=0; i<2 && bufptr; ++i) {
        va_list args;
        va_start(args, fmt);
        int cx = vsnprintf(bufptr, bufsz, fmt, args);
        va_end(args);

        if ((cx >= 0 && (size_t)cx < bufsz) || cx < 0) {
            log_callback(bufptr);
            break;
        }

        if (bufptr == stackbuf) {
            bufsz = cx + 1;
            bufptr = new (std::nothrow) char[bufsz];
            if (!bufptr) log_callback("out of memory");
        }
        else {
            log_callback(bufptr);
            break;
        }
    }

    if (bufptr && bufptr != stackbuf) delete [] bufptr;
}

void MDMA::bug(const char *file, int line) const {
    log("Forbidden condition met in %s on line %d.", file, line);
}

void MDMA::die(const char *file, int line) const {
    bug(file, line);
    fflush(nullptr);
    raise(SIGSEGV);
}

const std::string *MDMA::assemble(
    const char *html, size_t html_len, const char *md, size_t md_len
) {
    if (!html || !md) {
        bug();
        return nullptr;
    }

    assembly_buffer.assign(html, html_len);

    std::map<std::string, int> identifiers;

    bool fail = (
        !parse_framework(
            assembly_buffer,
            [&identifiers](const TidyNode &node) {
                TidyAttr attr = tidyAttrGetById(node, TidyAttr_ID);
                const char *id = nullptr;

                if (attr && (id = tidyAttrValue(attr))) {
                    identifiers.emplace(id, 0);
                }
            }
        )
    );

    if (fail) return nullptr;

    int next_id = 1;
    std::list<tinyxml2::XMLDocument> sections;
    std::map<int, std::pair<int, std::string>> headings;

    {
        std::map<int, int> level_to_id;

        fail = (
            !parse_markdown(
                md, md_len, sections,
                [&](const tinyxml2::XMLElement &heading) {
                    const char *title = heading.GetText();
                    const char *name = heading.Name();

                    if (!title || !name) return 0;

                    int level = (
                        !strcasecmp("h1", name) ? 1 :
                        !strcasecmp("h2", name) ? 2 :
                        !strcasecmp("h3", name) ? 3 :
                        !strcasecmp("h4", name) ? 4 :
                        !strcasecmp("h5", name) ? 5 :
                        !strcasecmp("h6", name) ? 6 : 7
                    );

                    int id = next_id++;

                    headings.emplace(
                        id,
                        std::make_pair(
                            add_heading_and_get_parent_id(
                                id, level, level_to_id
                            ),
                            std::string(title)
                        )
                    );

                    return id;
                }
            )
        );
    }

    if (fail) return nullptr;

    if (!fill_framework(assembly_buffer, sections, headings)) {
        return nullptr;
    }

    return &assembly_buffer;
}

bool MDMA::parse_framework(
    std::string &framework,
    std::function<void(const TidyNode &node)> callback
) {
    TidyBuffer tidy_buffer{};   // TODO: assure automatic deallocation on return
    TidyDoc tdoc = tidyCreate();// TODO: assure automatic deallocation on return

    setup(tdoc);
    tidyOptSetBool(tdoc, TidyIndentContent, yes);
    tidyParseString(tdoc, framework.c_str());

    do {
        TidyNode found = find_if(
            tidyGetBody(tdoc),
            [](const TidyNode &node, int) {
                TidyAttr attr = tidyAttrGetById(node, TidyAttr_ID);

                if (!attr
                || (strcasecmp(tidyAttrValue(attr), "MDMA-AGENDA")
                &&  strcasecmp(tidyAttrValue(attr), "MDMA-CONTENT"))) {
                    return false;
                }

                return tidyGetChild(node) != nullptr;
            }
        );

        if (!found) break;

        TidyNode child = tidyGetChild(found);

        for (; child; child = tidyGetChild(found)) {
            tidyDiscardElement(tdoc, child);
        }
    }
    while (true);

    do {
        TidyNode style = find_if(
            tidyGetHead(tdoc),
            [](const TidyNode &node, int) {
                if (tidyNodeGetId(node) != TidyTag_STYLE) {
                    return false;
                }

                TidyAttr attr = tidyAttrGetById(node, TidyAttr_CLASS);

                if (attr
                && !strcasecmp(tidyAttrValue(attr), "MDMA-AUTOGENERATED")) {
                    return true;
                }

                return false;
            }
        );

        if (!style) break;

        tidyDiscardElement(tdoc, style);
    }
    while (true);

    find_if(
        tidyGetRoot(tdoc),
        [&](const TidyNode &node, int) {
            callback(node);
            return false;
        }
    );

    tidyCleanAndRepair(tdoc);
    tidySaveBuffer(tdoc, &tidy_buffer);

    framework.assign((const char *) tidy_buffer.bp, tidy_buffer.size);

    tidyRelease(tdoc);
    tidyBufFree(&tidy_buffer);

    return true;
}

bool MDMA::parse_markdown(
    const char *md, size_t md_len, std::list<tinyxml2::XMLDocument> &sections,
    std::function<int(const tinyxml2::XMLElement &heading)> callback
) {
    std::string xhtml;

    bool fail = md_html(
        md, MD_SIZE(md_len),
        [](const MD_CHAR *str, MD_SIZE len, void *userdata) {
            std::string *dest = static_cast<std::string *>(userdata);
            dest->append(str, len);
        },
        &xhtml, MD_DIALECT_GITHUB, MD_HTML_FLAG_XHTML
    );

    if (fail) {
        bug();
        return false;
    }

    tinyxml2::XMLDocument doc;
    doc.Parse(xhtml.c_str(), xhtml.size());
    tinyxml2::XMLElement *parent = doc.RootElement();

    if (!parent) {
        bug();
        return false;
    }

    const tinyxml2::XMLNode *first_heading = find_if(
        *parent,
        [](const tinyxml2::XMLNode &node, int depth) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (name) {
                if (!strcasecmp("h1", name)) {
                    return true;
                }
            }

            return false;
        }
    );

    const tinyxml2::XMLNode *sibling = first_heading;

    for (; sibling; sibling = sibling->NextSibling()) {
        const tinyxml2::XMLElement *sibling_element = sibling->ToElement();

        bool new_tab = false;
        bool new_anchor = false;

        if (sibling_element) {
            const char *name = sibling_element->Name();

            if (!name) {
                continue;
            }

            new_tab = !strcasecmp("h1", name);

            new_anchor = (
                new_tab
                || !strcasecmp("h2", name)
                || !strcasecmp("h3", name)
                || !strcasecmp("h4", name)
                || !strcasecmp("h5", name)
                || !strcasecmp("h6", name)
            );
        }

        if (new_tab) {
            tinyxml2::XMLNode *node = nullptr;
            tinyxml2::XMLElement *elem = nullptr;

            sections.emplace_back();

            node = sections.back().InsertFirstChild(
                sections.back().NewElement("article")
            );

            elem = node ? node->ToElement() : nullptr;

            if (elem) {
                elem->SetAttribute("class", "tab");
            }
        }

        if (sections.empty()) continue;

        tinyxml2::XMLElement *root = sections.back().RootElement();

        tinyxml2::XMLNode *node = (
            new_anchor ? (
                sibling->ShallowClone(&sections.back())
            ) : sibling->DeepClone(&sections.back())
        );

        tinyxml2::XMLElement *elem = node ? node->ToElement() : nullptr;

        if (!elem) {
            die();
        }

        root->InsertEndChild(elem);

        if (!new_anchor) {
            continue;
        }

        node = elem->InsertEndChild(sections.back().NewElement("a"));

        elem = node ? node->ToElement() : nullptr;

        if (!elem) {
            die();
        }

        int id = callback(*sibling_element);

        if (id <= 0) {
            die();
        }

        std::string anchor_id(
            std::string("anchor-").append(std::to_string(id))
        );

        elem->SetAttribute("id", anchor_id.c_str());
        elem->SetAttribute(
            "href", std::string("#").append(anchor_id).c_str()
        );

        const tinyxml2::XMLNode *child = sibling->FirstChild();

        for (; child; child = child->NextSibling()) {
            tinyxml2::XMLNode *n = child->DeepClone(&sections.back());

            if (!n) {
                die();
            }

            elem->InsertEndChild(n);
        }
    }

    return true;
}


bool MDMA::fill_framework(
    std::string &framework, const std::list<tinyxml2::XMLDocument> &sections,
    const std::map<int, std::pair<int, std::string>> &headings
) {
    TidyBuffer tidy_buffer{};   // TODO: assure automatic deallocation on return
    TidyDoc tdoc = tidyCreate();// TODO: assure automatic deallocation on return

    setup(tdoc);

    if (!cfg.minify) {
        tidyOptSetInt(tdoc, TidyIndentContent, yes);
        tidyOptSetInt(tdoc, TidyIndentSpaces, 2);
        tidyOptSetInt(tdoc, TidyVertSpace, yes);
        tidyOptSetInt(tdoc, TidyWrapLen, 68);
    }

    tidyParseString(tdoc, framework.c_str());
    framework.clear();

    std::string agenda_css;
    size_t heading_counter = 0;
    std::map<int, std::vector<int>> heading_to_descendants;

    for (const auto &p : headings) {
        int parent_id = p.second.first;

        if (parent_id) {
            heading_to_descendants[parent_id].emplace_back(p.first);
        }

        std::string anchor_id(
            std::string("anchor-").append(std::to_string(p.first))
        );

        ++heading_counter;

        agenda_css.append("#MDMA-CONTENT:has(#").append(anchor_id).append(
            ":target) ~ .menu a[href=\"#"
        ).append(anchor_id).append("\"]");

        if (heading_counter == headings.size()) {
            agenda_css.append(" {\n").append(
                "    color: var(--MDMA-AGENDA-TARGET-COLOR);\n}\n"
            );
        }
        else {
            agenda_css.append(",\n");
        }
    }

    for (auto &p : heading_to_descendants) {
        std::vector<int> &descendants = p.second;

        for (size_t i=0; i<descendants.size(); ++i) {
            int descendant_id = descendants[i];

            if (heading_to_descendants.count(descendant_id)) {
                for (int id : heading_to_descendants[descendant_id]) {
                    descendants.emplace_back(id);
                }
            }
        }
    }

    while (!heading_to_descendants.empty()) {
        int heading_id = heading_to_descendants.begin()->first;
        std::vector<int> &descendants = heading_to_descendants.at(heading_id);

        agenda_css.append("#MDMA-CONTENT:not(\n");
        agenda_css.append("    :has(#anchor-").append(
            std::to_string(heading_id)
        ).append(":target),\n");

        while (!descendants.empty()) {
            int descendant_id = descendants.back();

            agenda_css.append("    :has(#anchor-").append(
                std::to_string(descendant_id)
            ).append(":target)");

            descendants.pop_back();

            if (descendants.empty()) {
                agenda_css.append("\n");
            }
            else {
                agenda_css.append(",\n");
            }
        }

        agenda_css.append(") ~ .menu > .options a[href=\"#anchor-").append(
            std::to_string(heading_id)
        ).append("\"] + div");

        heading_to_descendants.erase(heading_id);

        if (heading_to_descendants.empty()) {
            agenda_css.append(
                " {\n"
                "    max-height: 0;\n"
                "    transition: max-height 0.2s ease-out;\n"
                "}\n"
            );
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

                        framework.append(
                            "<meta name=\"generator\" content=\""
                        ).append(MDMA::CAPTION).append(" version ").append(
                            MDMA::VERSION
                        ).append("\">\n");

                        if (!headings.empty()) {
                            end_tags[depth].append(
                                "<style class=\"MDMA-AUTOGENERATED\">\n"
                            ).append(
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

                    if (tidyNodeGetId(node) == TidyTag_META) {
                        TidyAttr attr = tidyAttrGetById(node, TidyAttr_NAME);
                        const char *name = nullptr;
                        const char *content = nullptr;
                        const char *prefix = MDMA::CAPTION;

                        if (attr
                        && (name = tidyAttrValue(attr))
                        && !strcasecmp("generator", name)
                        && (attr = tidyAttrGetById(node, TidyAttr_CONTENT))
                        && (content = tidyAttrValue(attr))
                        && !strncasecmp(prefix, content, strlen(prefix))) {
                            // Since we are robustly adding a generator META
                            // tag to the HTML head we have to reject any
                            // existing tags here to prevent them from
                            // accumulating when feeding the program its own
                            // output.

                            break;

                        }
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

    return true;
}

int MDMA::add_heading_and_get_parent_id(
    int id, int level, std::map<int, int> &level_to_id
) const {
    int last_id = 0;

    while (!level_to_id.empty()) {
        int last_level = std::prev(level_to_id.end())->first;

        if (last_level >= level) {
            level_to_id.erase(last_level);
            continue;
        }

        last_id = level_to_id.at(last_level);
        break;
    }

    level_to_id[level] = id;

    return last_id;
}


void MDMA::setup(TidyDoc doc) const {
    tidyOptSetBool(doc, TidyMark, no);
    tidyOptSetBool(doc, TidyMergeSpans, no);
    tidyOptSetBool(doc, TidyDropEmptyElems, no);
    tidyOptSetBool(doc, TidyDropEmptyParas, no);

    tidyOptSetBool(doc, TidyIndentAttributes, no);
    tidyOptSetBool(doc, TidyIndentCdata, no);
    tidyOptSetInt(doc, TidyIndentContent, no );
    tidyOptSetInt(doc, TidyIndentSpaces, 0);
    tidyOptSetInt(doc, TidyVertSpace, TidyAutoState);
    tidyOptSetInt(doc, TidyWrapLen, 0);
}

tinyxml2::XMLNode *MDMA::find_if(
    const tinyxml2::XMLNode &root,
    std::function<bool(const tinyxml2::XMLNode &, int)> fun
) const {
    int depth = 0;

    const tinyxml2::XMLNode *parent = &root;

    while (parent) {
        if (fun(*parent, depth)) {
            return const_cast<tinyxml2::XMLNode *>(parent);
        }

        const tinyxml2::XMLNode *child = parent->FirstChild();

        if (child) {
            parent = child;
            ++depth;
            continue;
        }

        const tinyxml2::XMLNode *sibling = parent->NextSibling();

        if (sibling) {
            parent = sibling;
            continue;
        }

        do {
            parent = parent->Parent();

            if (!parent) break;

            sibling = parent->NextSibling();

            --depth;
        }
        while (!sibling);

        parent = sibling;
    }

    return nullptr;
}

TidyNode MDMA::find_if(
    const TidyNode root, std::function<bool(const TidyNode &, int)> fun
) const {
    int depth = 0;

    TidyNode parent = root;

    while (parent) {
        if (fun(parent, depth)) {
            return const_cast<TidyNode>(parent);
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

void MDMA::assemble_framework_body(
    std::string &body_html, const std::list<tinyxml2::XMLDocument> &sections,
    const std::map<int, std::pair<int, std::string>> &headings
) const {
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

        setup(tdoc_xml);
        tidyOptSetBool(tdoc_xml, TidyXmlOut, yes);
        tidyParseString(tdoc_xml, html5.c_str());

        TidyNode body_node = tidyGetBody(tdoc_xml);
        tidyNodeGetText(tdoc_xml, body_node, &tidy_buffer);

        doc.Parse((const char *) tidy_buffer.bp, tidy_buffer.size);

        tidyRelease(tdoc_xml);
        tidyBufFree(&tidy_buffer);
    }

    tinyxml2::XMLElement *parent = doc.RootElement();

    tinyxml2::XMLNode *content_node = find_if(
        *parent,
        [](const tinyxml2::XMLNode &node, int) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (!name || strcasecmp("div", name)) {
                return false;
            }

            const char *id = "";

            if (el->QueryStringAttribute("id", &id) != tinyxml2::XML_SUCCESS) {
                return false;
            }

            if (id && !strcasecmp("MDMA-CONTENT", id)) {
                return true;
            }

            return false;
        }
    );

    tinyxml2::XMLElement *content = (
        content_node ? content_node->ToElement() : nullptr
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

    tinyxml2::XMLNode *agenda_node = find_if(
        *parent,
        [](const tinyxml2::XMLNode &node, int) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (!name || strcasecmp("div", name)) {
                return false;
            }

            const char *id = "";

            if (el->QueryStringAttribute("id", &id) != tinyxml2::XML_SUCCESS) {
                return false;
            }

            if (id && !strcasecmp("MDMA-AGENDA", id)) {
                return true;
            }

            return false;
        }
    );

    tinyxml2::XMLElement *agenda = (
        agenda_node ? agenda_node->ToElement() : nullptr
    );

    if (agenda) {
        agenda->DeleteChildren();

        std::map<int, tinyxml2::XMLElement*> groups{{0, agenda}};

        for (const auto &p : headings) {
            int parent_id = p.second.first;
            tinyxml2::XMLElement *container = std::prev(groups.end())->second;

            if (groups.count(parent_id)) {
                container = groups.at(parent_id);

                while (!groups.empty()) {
                    int group_id = std::prev(groups.end())->first;

                    if (group_id != parent_id) {
                        groups.erase(group_id);
                        continue;
                    }

                    break;
                }
            }
            else {
                tinyxml2::XMLNode *node = container->InsertEndChild(
                    doc.NewElement("div")
                );

                tinyxml2::XMLElement *elem = node ? node->ToElement() : nullptr;

                if (elem) {
                    groups.emplace(parent_id, elem);
                    container = elem;
                }
                else die();
            }

            tinyxml2::XMLNode *node = container->InsertEndChild(
                doc.NewElement("a")
            );

            tinyxml2::XMLElement *elem = node ? node->ToElement() : nullptr;

            if (elem) {
                elem->SetAttribute(
                    "href",
                    std::string("#anchor-").append(
                        std::to_string(p.first)
                    ).c_str()
                );

                elem->InsertNewText(p.second.second.c_str());
            }
            else die();
        }
    }

    find_if(
        *parent,
        [](const tinyxml2::XMLNode &node, int) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (!name || (strcasecmp("td", name) && strcasecmp("th", name))) {
                return false;
            }

            const char *align = "";
            tinyxml2::XMLError err = el->QueryStringAttribute("align", &align);

            if (err != tinyxml2::XML_SUCCESS) {
                return false;
            }

            if (align
            && strcasecmp("left", align)
            && strcasecmp("right", align)
            && strcasecmp("center", align)) {
                return false;
            }

            tinyxml2::XMLElement *fix_el{
                const_cast<tinyxml2::XMLElement *>(el)
            };

            fix_el->SetAttribute(
                "style",
                std::string("text-align: ").append(align).append(";").c_str()
            );

            fix_el->DeleteAttribute("align");

            return false;
        }
    );

    tinyxml2::XMLPrinter printer(nullptr, true);
    doc.Print(&printer);

    body_html.assign(printer.CStr());

    {
        // Let's convert XML to HTML.

        std::string html5(smallest_valid_html5);
        html5.append(body_html);

        TidyBuffer tidy_buffer{};
        TidyDoc tdoc = tidyCreate();

        setup(tdoc);
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

#endif
