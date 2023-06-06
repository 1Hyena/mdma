// SPDX-License-Identifier: MIT
#ifndef MDMA_H_02_06_2023
#define MDMA_H_02_06_2023

#include "slugify.h"
////////////////////////////////////////////////////////////////////////////////
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
    , htmltidy_buffer{}
    , log_callback(nullptr) {
        tidyBufInit(&htmltidy_buffer);
    }

    ~MDMA() {
        tidyBufFree(&htmltidy_buffer);
    }

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

    bool prune_framework(TidyDoc framework);
    bool parse_markdown(const char *str, size_t len);
    bool fill_framework(TidyDoc framework);

    void add_heading(
        int id, int level, const char *title, std::map<int, int> &level_to_id
    );

    void setup(TidyDoc) const;

    TidyNode find_if(
        TidyNode root, std::function<bool(const TidyNode &, int)> fun
    ) const;

    tinyxml2::XMLNode *find_if(
        const tinyxml2::XMLNode &root,
        std::function<bool(const tinyxml2::XMLNode &, int)> fun
    ) const;

    void assemble_framework_body(std::string &body_html);

    static std::string bin2safe(unsigned char *bytes, size_t len);

    struct heading_data {
        int *parent_id;
        std::string *title;
        std::string *identifier;
    };

    const heading_data *get_heading_data(int id) const;

    std::string assembly_buffer;
    TidyBuffer  htmltidy_buffer;
    std::function<void(const char *text)> log_callback;
    std::map<std::string, int> identifiers;
    std::list<tinyxml2::XMLDocument> sections;
    std::map<
        int,
        std::tuple<
            heading_data,
            int, std::string, std::string
        >
    > headings;
};

const std::string *MDMA::assemble(
    const char *html, size_t html_len, const char *md, size_t md_len
) {
    const std::string *result = nullptr;

    if (!html || !md) {
        bug();
        return result;
    }

    assembly_buffer.assign(html, html_len);
    identifiers.clear();
    sections.clear();

    {
        TidyDoc tdoc = tidyCreate();

        {
            setup(tdoc);
            tidyParseString(tdoc, assembly_buffer.c_str());

            if (prune_framework(tdoc)
            &&  parse_markdown(md, md_len)
            &&  fill_framework(tdoc)) {
                result = &assembly_buffer;
            }
        }

        tidyRelease(tdoc);
    }

    return result;
}

bool MDMA::prune_framework(TidyDoc framework) {
    do {
        TidyNode found = find_if(
            tidyGetBody(framework),
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
            tidyDiscardElement(framework, child);
        }
    }
    while (true);

    do {
        TidyNode style = find_if(
            tidyGetHead(framework),
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

        tidyDiscardElement(framework, style);
    }
    while (true);

    find_if(
        tidyGetRoot(framework),
        [&](const TidyNode &node, int) {
            TidyAttr attr = tidyAttrGetById(node, TidyAttr_ID);
            const char *id = nullptr;

            if (attr && (id = tidyAttrValue(attr))) {
                this->identifiers.emplace(id, 0);
            }

            return false;
        }
    );

    return true;
}

bool MDMA::parse_markdown(const char *md, size_t md_len) {
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
    int next_id = 1;
    std::map<int, int> level_to_id;

    headings.clear();

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

        int id = 0;

        {
            const char *title = sibling_element->GetText();
            const char *name = sibling_element->Name();

            if (!title || !name) return 0;

            int level = (
                !strcasecmp("h1", name) ? 1 :
                !strcasecmp("h2", name) ? 2 :
                !strcasecmp("h3", name) ? 3 :
                !strcasecmp("h4", name) ? 4 :
                !strcasecmp("h5", name) ? 5 :
                !strcasecmp("h6", name) ? 6 : 7
            );

            id = next_id++;

            add_heading(id, level, title, level_to_id);
        }

        if (id <= 0) {
            die();
        }

        if (!get_heading_data(id)) {
            die();
        }

        std::string *anchor_id = get_heading_data(id)->identifier;

        elem->SetAttribute("id", anchor_id->c_str());
        elem->SetAttribute(
            "href", std::string("#").append(*anchor_id).c_str()
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


bool MDMA::fill_framework(TidyDoc framework) {
    assembly_buffer.clear();

    std::string agenda_css;
    size_t heading_counter = 0;
    std::map<int, std::vector<int>> heading_to_descendants;

    for (const auto &p : headings) {
        int parent_id = *(std::get<0>(p.second).parent_id);

        if (parent_id) {
            heading_to_descendants[parent_id].emplace_back(p.first);
        }

        std::string *anchor_id = std::get<0>(p.second).identifier;

        ++heading_counter;

        agenda_css.append("#MDMA-CONTENT:has(#").append(*anchor_id).append(
            ":target) ~ .menu a[href=\"#"
        ).append(*anchor_id).append("\"]");

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

        if (!get_heading_data(heading_id)) die();

        agenda_css.append("#MDMA-CONTENT:not(\n");
        agenda_css.append("    :has(#").append(
            *(get_heading_data(heading_id)->identifier)
        ).append(":target),\n");

        while (!descendants.empty()) {
            int descendant_id = descendants.back();

            if (!get_heading_data(descendant_id)) die();

            agenda_css.append("    :has(#").append(
                *(get_heading_data(descendant_id)->identifier)
            ).append(":target)");

            descendants.pop_back();

            if (descendants.empty()) {
                agenda_css.append("\n");
            }
            else {
                agenda_css.append(",\n");
            }
        }

        agenda_css.append(") ~ .menu > .options a[href=\"#").append(
            *(get_heading_data(heading_id)->identifier)
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
        tidyGetRoot(framework),
        [&](const TidyNode &node, int depth) {
            while (!end_tags.empty()) {
                int d = end_tags.begin()->first;

                if (depth <= d) {
                    assembly_buffer.append(end_tags.begin()->second);
                    end_tags.erase(d);
                }
                else break;
            }

            switch (depth) {
                case 1: {
                    if (tidyNodeGetId(node) != TidyTag_HTML) {
                        tidyBufClear(&htmltidy_buffer);
                        tidyNodeGetText(framework, node, &htmltidy_buffer);
                        assembly_buffer.append(
                            (const char *) htmltidy_buffer.bp,
                            htmltidy_buffer.size
                        );
                    }
                    else {
                        assembly_buffer.append("<html>\n");
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
                        assembly_buffer.append("<head>\n");

                        assembly_buffer.append(
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
                        tidyBufClear(&htmltidy_buffer);
                        tidyNodeGetText(framework, node, &htmltidy_buffer);
                        std::string body_html(
                            (const char *) htmltidy_buffer.bp,
                            htmltidy_buffer.size
                        );

                        assemble_framework_body(body_html);

                        assembly_buffer.append(body_html);
                    }
                    else {
                        tidyBufClear(&htmltidy_buffer);
                        tidyNodeGetText(framework, node, &htmltidy_buffer);
                        assembly_buffer.append(
                            (const char *) htmltidy_buffer.bp,
                            htmltidy_buffer.size
                        );
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

                    tidyBufClear(&htmltidy_buffer);
                    tidyNodeGetText(framework, node, &htmltidy_buffer);
                    assembly_buffer.append(
                        (const char *) htmltidy_buffer.bp,
                        htmltidy_buffer.size
                    );

                    break;
                }
                default: break;
            }

            return false;
        }
    );

    for (const auto &p : end_tags) {
        assembly_buffer.append(p.second);
    }

    if (!cfg.minify) {
        tidyOptSetInt(framework, TidyIndentContent, yes);
        tidyOptSetInt(framework, TidyIndentSpaces, 2);
        tidyOptSetInt(framework, TidyVertSpace, yes);
        tidyOptSetInt(framework, TidyWrapLen, 68);
    }

    tidyParseString(framework, assembly_buffer.c_str());
    tidyCleanAndRepair(framework);

    tidyBufClear(&htmltidy_buffer);
    tidySaveBuffer(framework, &htmltidy_buffer);
    assembly_buffer.assign(
        (const char *) htmltidy_buffer.bp, htmltidy_buffer.size
    );

    return true;
}

void MDMA::add_heading(
    int id, int level, const char *title, std::map<int, int> &level_to_id
) {
    int parent_id = 0;

    while (!level_to_id.empty()) {
        int last_level = std::prev(level_to_id.end())->first;

        if (last_level >= level) {
            level_to_id.erase(last_level);
            continue;
        }

        parent_id = level_to_id.at(last_level);
        break;
    }

    level_to_id[level] = id;


    std::string slug{slugify(title)};
    std::string suffix;

    if (slug.empty()) {
        slug.assign("anchor");
    }

    for (size_t i=1; i<10000; ++i) {
        if (!identifiers.emplace(std::string(slug).append(suffix), id).second) {
            if (slug.back() != '-') {
                slug.append("-");
            }

            suffix.assign(std::to_string(i));
            suffix.assign(
                bin2safe((unsigned char*) suffix.data(), suffix.size())
            );

            continue;
        }

        slug.assign(std::string(slug).append(suffix));

        auto p = headings.emplace(
            id,
            std::make_tuple(heading_data{}, parent_id, std::string(title), slug)
        );

        if (p.second) {
            heading_data *data = &std::get<0>(p.first->second);
            data->parent_id    = &std::get<1>(p.first->second);
            data->title        = &std::get<2>(p.first->second);
            data->identifier   = &std::get<3>(p.first->second);
        }
        else break;

        return;
    }

    die();
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

void MDMA::assemble_framework_body(std::string &body_html) {
    static constexpr const char *smallest_valid_html5{
        "<!DOCTYPE html><title>x</title>"
    };

    tinyxml2::XMLDocument doc;

    {
        // Let's convert HTML to XML.

        std::string html5(smallest_valid_html5);
        html5.append(body_html);

        TidyDoc tdoc_xml = tidyCreate();

        setup(tdoc_xml);
        tidyOptSetBool(tdoc_xml, TidyXmlOut, yes);
        tidyParseString(tdoc_xml, html5.c_str());

        TidyNode body_node = tidyGetBody(tdoc_xml);

        tidyBufClear(&htmltidy_buffer);
        tidyNodeGetText(tdoc_xml, body_node, &htmltidy_buffer);
        doc.Parse((const char *) htmltidy_buffer.bp, htmltidy_buffer.size);

        tidyRelease(tdoc_xml);
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
            int parent_id = *(std::get<0>(p.second).parent_id);
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
                    std::string("#").append(
                        std::get<0>(p.second).identifier->c_str()
                    ).c_str()
                );

                elem->InsertNewText(std::get<0>(p.second).title->c_str());
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

        TidyDoc tdoc = tidyCreate();

        setup(tdoc);
        tidyParseString(tdoc, html5.c_str());

        TidyNode body = tidyGetBody(tdoc);

        tidyBufClear(&htmltidy_buffer);
        tidyNodeGetText(tdoc, body, &htmltidy_buffer);
        body_html.assign(
            (const char *) htmltidy_buffer.bp, htmltidy_buffer.size
        );

        tidyRelease(tdoc);
    }
}

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

std::string MDMA::bin2safe(unsigned char *bytes, size_t len) {
    static const char *symbols =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static size_t symlen = std::strlen(symbols);
    std::string result;

    for (size_t i=0; i<len; ++i) {
        unsigned char b = bytes[i];
        size_t index = b % symlen;
        result.append(1, symbols[index]);
    }

    return result;
}

const MDMA::heading_data *MDMA::get_heading_data(int id) const {
    if (!headings.count(id)) return nullptr;

    return &(std::get<0>(headings.at(id)));
}

#endif
