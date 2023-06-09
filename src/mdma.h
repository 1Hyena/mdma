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
#include <uriparser/Uri.h>
#include <Imlib2.h>
#include <signal.h>
#include <filesystem>
#include <sys/mman.h>
#include <b64/encode.h>
#include <b64/decode.h>
#include <fstream>

class MDMA {
    public:
    static constexpr const char *CAPTION = "MarkDown Monolith Assembler";
    static constexpr const char *VERSION = "1.0";
    static constexpr const char *AUTHOR  = "Erich Erstu";
    static constexpr const char *smallest_valid_html5{
        "<!DOCTYPE html><title>x</title>"
    };

    MDMA() : cfg(
        {
            .github = false,
            .minify = false,
            .verbose= false
        }
    )
    , directory("")
    , assembly_buffer("")
    , htmltidy_buffer{}
    , log_callback(nullptr) {
        tidyBufInit(&htmltidy_buffer);
    }

    ~MDMA() {
        tidyBufFree(&htmltidy_buffer);
    }

    struct cfg_type {
        bool github:1;
        bool minify:1;
        bool verbose:1;
    } cfg;

    void set_logger(const std::function<void(const char *)>& log_callback);
    void set_directory(const std::filesystem::path &);

    const std::string *assemble(
        const char *htm, size_t htm_sz, const char *md, size_t md_sz
    );

    static std::string uri_param_value(const char *uri, const char *key);

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

    tinyxml2::XMLElement *find_element(
        const tinyxml2::XMLNode &root, const char *id
    ) const;

    std::string assemble_framework_body(const char *body_html);

    void install_content(tinyxml2::XMLDocument &) const;
    void install_agenda (tinyxml2::XMLDocument &) const;
    void install_loader (tinyxml2::XMLDocument &) const;
    void enhance_tables (tinyxml2::XMLDocument &) const;
    void enhance_videos (tinyxml2::XMLDocument &) const;
    void enhance_images (tinyxml2::XMLDocument &);

    void enhance_image(tinyxml2::XMLElement &element);

    void html2xml(const std::string &html, tinyxml2::XMLDocument &xml);
    std::string xml2html(const char *xml);

    std::vector<unsigned char> load_file(const char *);
    std::vector<unsigned char> decode_base64(const char *);
    std::vector<unsigned char> decode_base64(const char *, size_t);
    std::vector<unsigned char> dump_image(const Imlib_Image &) const;

    std::string encode_base64(const unsigned char *, size_t);

    static std::string bin2safe(unsigned char *bytes, size_t len);

    struct heading_data {
        int *parent_id;
        std::string *title;
        std::string *identifier;
    };

    const heading_data *get_heading_data(int id) const;

    std::filesystem::path directory;
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
        &xhtml,
        cfg.github ? MD_DIALECT_GITHUB : MD_DIALECT_COMMONMARK,
        MD_HTML_FLAG_XHTML
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
        elem->SetAttribute("target", "_self");

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

    std::string agenda_css{
        ":root {--MDMA-LOADER-OPACITY: 100%; --MDMA-PAGE-LOADED: 0%;}\n"
    };

    size_t heading_counter = 0;
    std::map<int, std::vector<int>> heading_to_descendants;

    for (const auto &p : headings) {
        int parent_id = *(std::get<0>(p.second).parent_id);

        if (parent_id) {
            heading_to_descendants[parent_id].emplace_back(p.first);
        }

        std::string *anchor_id = std::get<0>(p.second).identifier;

        ++heading_counter;

        agenda_css.append("body:has(#").append(*anchor_id).append(
            ":target) #MDMA-AGENDA a[href=\"#"
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

        agenda_css.append("body:not(\n");
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

        agenda_css.append(") #MDMA-AGENDA a[href=\"#").append(
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

                        assembly_buffer.append(
                            assemble_framework_body(
                                std::string(
                                    (const char *) htmltidy_buffer.bp,
                                    htmltidy_buffer.size
                                ).c_str()
                            )
                        );
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
    tidyOptSetBool(doc, TidyMuteShow, yes);
    tidyOptSetValue(
        doc, TidyMuteReports,
        "STRING_MUTING_TYPE,FOUND_STYLE_IN_BODY"
    );

    tidyOptSetBool(doc, TidyMark, no);
    tidyOptSetBool(doc, TidyMergeSpans, no);
    tidyOptSetBool(doc, TidyDropEmptyElems, no);
    tidyOptSetBool(doc, TidyDropEmptyParas, no);
    tidyOptSetBool(doc, TidyStyleTags, no);

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

tinyxml2::XMLElement *MDMA::find_element(
    const tinyxml2::XMLNode &root, const char *id
) const {
    tinyxml2::XMLNode *found = find_if(
        root, [&id](const tinyxml2::XMLNode &node, int) {
            const tinyxml2::XMLElement *e = node.ToElement();
            const char *eid = "";

            if (e == nullptr
            ||  e->QueryStringAttribute("id", &eid) != tinyxml2::XML_SUCCESS) {
                return false;
            }

            return eid && id && !strcmp(eid, id);
        }
    );

    return found ? found->ToElement() : nullptr;
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

void MDMA::install_content(tinyxml2::XMLDocument &doc) const {
    tinyxml2::XMLElement *root = doc.RootElement();
    tinyxml2::XMLElement *content = find_element(*root, "MDMA-CONTENT");

    if (!content) return;

    content->DeleteChildren();

    for (const tinyxml2::XMLDocument &section : sections) {
        const tinyxml2::XMLElement *root = section.RootElement();
        tinyxml2::XMLNode *node = root->DeepClone(&doc);

        if (node) {
            content->InsertEndChild(node);
        }
    }
}

void MDMA::install_agenda(tinyxml2::XMLDocument &doc) const {
    tinyxml2::XMLElement *root = doc.RootElement();
    tinyxml2::XMLElement *agenda = find_element(*root, "MDMA-AGENDA");

    if (!agenda) return;

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
            elem->SetAttribute("target", "_self");
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

void MDMA::install_loader(tinyxml2::XMLDocument &doc) const {
    tinyxml2::XMLElement *root = doc.RootElement();

    std::vector<tinyxml2::XMLElement *> checkpoints;

    find_if(
        *root,
        [&checkpoints](const tinyxml2::XMLNode &node, int node_depth) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (!name) return false;

            bool checkpoint{
                !strcasecmp("h1",   name) ||
                !strcasecmp("h2",   name) ||
                !strcasecmp("h3",   name) ||
                !strcasecmp("h4",   name) ||
                !strcasecmp("h5",   name) ||
                !strcasecmp("h6",   name)
            };

            if (checkpoint) {
                checkpoints.emplace_back(
                    const_cast<tinyxml2::XMLElement *>(el)
                );
            }

            return false;
        }
    );

    for (size_t i=0; i<checkpoints.size(); ++i) {
        tinyxml2::XMLElement *el = checkpoints[i];
        tinyxml2::XMLNode *el_parent = el->Parent();

        tinyxml2::XMLNode *style{
            el_parent->InsertAfterChild(el, doc.NewElement("style"))
        };

        tinyxml2::XMLElement *style_el = style ? style->ToElement() : nullptr;

        if (style_el) {
            style_el->SetAttribute("class", "MDMA-AUTOGENERATED");
            style_el->SetText(
                std::string(":root {--MDMA-PAGE-LOADED: ").append(
                    std::to_string((100*(i+1)) / checkpoints.size())
                ).append("%;}").c_str()
            );
        }
    }

    tinyxml2::XMLNode *end_style{
        root->InsertEndChild(doc.NewElement("style"))
    };

    tinyxml2::XMLElement *style_el{
        end_style ? end_style->ToElement() : nullptr
    };

    if (style_el) {
        style_el->SetAttribute("class", "MDMA-AUTOGENERATED");
        style_el->SetText(
            std::string(":root {--MDMA-LOADER-OPACITY: 0%;}").c_str()
        );
    }
}

void MDMA::enhance_tables(tinyxml2::XMLDocument &doc) const {
    find_if(
        *doc.RootElement(),
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
}

void MDMA::enhance_videos(tinyxml2::XMLDocument &doc) const {
    static constexpr const std::string_view video_prefix{
        "https://www.youtube.com/watch?"
    };

    std::vector<tinyxml2::XMLNode *> video_links;

    find_if(
        *doc.RootElement(),
        [&video_links](const tinyxml2::XMLNode &node, int) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (!name || strcasecmp("a", name)) {
                return false;
            }

            const char *href = el->Attribute("href");

            if (href
            && !strncasecmp(href, video_prefix.data(), video_prefix.size())) {
                const tinyxml2::XMLNode *child = node.FirstChild();

                if (child && child == node.LastChild()) {
                    const tinyxml2::XMLElement *child_el = child->ToElement();
                    const char *child_name{
                        child_el ? child_el->Name() : nullptr
                    };

                    if (child_name
                    && !strcasecmp("img", child_name)
                    && child->NoChildren()) {
                        video_links.emplace_back(
                            const_cast<tinyxml2::XMLNode *>(&node)
                        );
                    }
                }
            }

            if (el->Attribute("target")) {
                return false;
            }

            tinyxml2::XMLElement *fix_el{
                const_cast<tinyxml2::XMLElement *>(el)
            };

            fix_el->SetAttribute("target", "_blank");

            return false;
        }
    );

    while (!video_links.empty()) {
        tinyxml2::XMLNode *link = video_links.back();
        tinyxml2::XMLNode *link_parent = link->Parent();
        tinyxml2::XMLElement *link_el = link->ToElement();

        const char *link_href{
            link_el ? link_el->Attribute("href") : nullptr
        };

        std::string video_id{link_href ? uri_param_value(link_href, "v") : ""};

        tinyxml2::XMLNode *div{
            !video_id.empty() ? (
                link_parent->InsertAfterChild(link, doc.NewElement("div"))
            ) : nullptr
        };

        tinyxml2::XMLElement *div_el = div ? div->ToElement() : nullptr;

        if (div_el) {
            tinyxml2::XMLNode *node = link->DeepClone(&doc);

            if (node) {
                div->InsertEndChild(node);
            }

            div_el->SetAttribute("class", "MDMA-VIDEO-CONTAINER");

            tinyxml2::XMLNode *iframe{
                div->InsertAfterChild(node, doc.NewElement("iframe"))
            };

            tinyxml2::XMLElement *iframe_el{
                iframe ? iframe->ToElement() : nullptr
            };

            if (iframe_el) {
                iframe_el->SetAttribute(
                    "src",
                    std::string(
                        "https://www.youtube-nocookie.com/embed/"
                    ).append(video_id).c_str()
                );

                iframe_el->SetAttribute("loading", "lazy");
                iframe_el->SetAttribute("allowfullscreen", "");
            }

            doc.DeleteNode(link);
        }

        video_links.pop_back();
    }
}

void MDMA::enhance_image(tinyxml2::XMLElement &el) {
    if (!el.Attribute("loading")) {
        el.SetAttribute("loading", "lazy");
    }

    const char *src = el.Attribute("src");

    if (!src) return;

    std::vector<unsigned char> rawsrc{ load_file(src) };

    if (rawsrc.empty()) {
        return;
    }

    Imlib_Image img_src{
        imlib_load_image_mem("memimg", rawsrc.data(), rawsrc.size())
    };

    if (!img_src) {
        log("Error loading image: %.50s", src);
        return;
    }

    imlib_context_set_image(img_src);

    int src_w = imlib_image_get_width();
    int src_h = imlib_image_get_height();

    el.SetAttribute("width",  std::to_string(src_w).c_str());
    el.SetAttribute("height", std::to_string(src_h).c_str());

    int dst_w = src_w / 8;
    int dst_h = src_h / 8;

    if (dst_w > 0 && dst_h > 0
    && !el.Attribute("style") && !imlib_image_has_alpha()) {
        Imlib_Image img_dst{
            imlib_create_cropped_scaled_image(0, 0, src_w, src_h, dst_w, dst_h)
        };

        if (img_dst) {
            const char *src_fmt = imlib_image_format();

            imlib_context_set_image(img_dst);
            imlib_image_set_format(src_fmt);

            std::vector<unsigned char> rawdst{ dump_image(img_dst) };

            imlib_context_set_image(img_dst);
            imlib_free_image();

            if (!rawdst.empty()) {
                std::string base64{
                    encode_base64(rawdst.data(), rawdst.size())
                };

                if (!base64.empty()) {
                    std::string style(
                        std::string(
                            "background-size: cover;background-image: url('"
                        ).append(
                            "data:image/").append(
                                !strcasecmp("jpg", src_fmt) ? (
                                    "jpeg"
                                ) : src_fmt
                            ).append(
                            ";base64,"
                        ).append(base64).append("');")
                    );

                    el.SetAttribute("style", style.c_str());
                }
            }
        }
    }

    imlib_context_set_image(img_src);
    imlib_free_image();
}

void MDMA::enhance_images(tinyxml2::XMLDocument &doc) {
    std::vector<tinyxml2::XMLElement *> images;

    find_if(
        *doc.RootElement(),
        [&images](const tinyxml2::XMLNode &node, int) {
            const tinyxml2::XMLElement *el = node.ToElement();
            const char *name = el ? el->Name() : nullptr;

            if (!name || strcasecmp("img", name)) {
                return false;
            }

            images.emplace_back(const_cast<tinyxml2::XMLElement *>(el));

            return false;
        }
    );

    for (tinyxml2::XMLElement *el : images) {
        enhance_image(*el);
    }
}

std::string MDMA::assemble_framework_body(const char *body_html) {
    tinyxml2::XMLDocument doc;

    html2xml(std::string(smallest_valid_html5).append(body_html), doc);

    install_content(doc);
    install_agenda(doc);

    enhance_tables(doc);
    enhance_videos(doc);
    enhance_images(doc);

    install_loader(doc);

    tinyxml2::XMLPrinter printer(nullptr, true);
    doc.Print(&printer);

    return xml2html(
        std::string(smallest_valid_html5).append(printer.CStr()).c_str()
    );
}

void MDMA::set_logger(const std::function<void(const char *)>& log_cb) {
    log_callback = log_cb;
}

void MDMA::set_directory(const std::filesystem::path &path) {
    directory = path;
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

std::string MDMA::encode_base64(
    const unsigned char *bytes, size_t len
) {
    if (std::numeric_limits<int>::max() < len) die();

    std::vector<char> base64buf(2*len+4);
    base64::base64_encodestate b64e;
    base64::base64_init_encodestate(&b64e);

    int written = base64::base64_encode_block(
        (const char *) bytes, int(len), base64buf.data(), &b64e
    );

    if (written < 0) die();

    std::string encoded(base64buf.data(), written);

    written = base64::base64_encode_blockend(base64buf.data(), &b64e);

    if (written > 0) {
        encoded.append(base64buf.data(), written);
    }

    encoded.erase(
        std::remove_if(
            encoded.begin(), encoded.end(), ::isspace
        ),
        encoded.end()
    );

    return encoded;
}

std::vector<unsigned char> MDMA::decode_base64(const char *str, size_t len) {
    if (len > std::numeric_limits<int>::max()) die();

    size_t decoded_maxlen = len / 4 * 3 + 2;

    base64::base64_decodestate b64d;
    base64::base64_init_decodestate(&b64d);

    std::vector<unsigned char> decoded(decoded_maxlen);

    int decoded_len{
        base64::base64_decode_block(
            str, int(len), (char *) decoded.data(), &b64d
        )
    };

    decoded[decoded_len] = 0;

    return decoded;
}

std::vector<unsigned char> MDMA::decode_base64(const char *str) {
    return decode_base64(str, strlen(str));
}

void MDMA::html2xml(const std::string &html, tinyxml2::XMLDocument &doc) {
    TidyDoc tdoc_xml = tidyCreate();

    setup(tdoc_xml);
    tidyOptSetBool(tdoc_xml, TidyXmlOut, yes);
    tidyParseString(tdoc_xml, html.c_str());

    TidyNode body_node = tidyGetBody(tdoc_xml);

    tidyBufClear(&htmltidy_buffer);
    tidyNodeGetText(tdoc_xml, body_node, &htmltidy_buffer);
    doc.Parse((const char *) htmltidy_buffer.bp, htmltidy_buffer.size);

    tidyRelease(tdoc_xml);
}

std::string MDMA::xml2html(const char *xml) {
    TidyDoc tdoc = tidyCreate();

    setup(tdoc);
    tidyParseString(tdoc, xml);

    TidyNode body = tidyGetBody(tdoc);

    tidyBufClear(&htmltidy_buffer);
    tidyNodeGetText(tdoc, body, &htmltidy_buffer);
    tidyRelease(tdoc);

    return std::string(
        (const char *) htmltidy_buffer.bp, htmltidy_buffer.size
    );
}

const MDMA::heading_data *MDMA::get_heading_data(int id) const {
    if (!headings.count(id)) return nullptr;

    return &(std::get<0>(headings.at(id)));
}

std::string MDMA::uri_param_value(const char *uri_str, const char *key) {
    std::string result;
    UriUriA uri;
    const char *errorPos;

    if (uriParseSingleUriA(&uri, uri_str, &errorPos) != URI_SUCCESS) {
        return result;
    }

    UriQueryListA * list;
    int count;
    auto uri_result{
        uriDissectQueryMallocA(
            &list, &count, uri.query.first, uri.query.afterLast
        )
    };

    if (uri_result != URI_SUCCESS) {
        uriFreeUriMembersA(&uri);
        return result;
    }

    for (UriQueryListStructA *param = list; param; param = param->next) {
        if (!strcmp(key, param->key)) {
            result.assign(param->value);
            break;
        }
    }

    uriFreeQueryListA(list);
    uriFreeUriMembersA(&uri);

    return result;
}

std::vector<unsigned char> MDMA::load_file(const char *src) {
    static constexpr struct prefix_type{
        const std::string_view data;
        const std::string_view http;
        const std::string_view https;
    } prefixes{
        .data { "data:"    },
        .http { "http://"  },
        .https{ "https://" }
    };

    if (cfg.verbose) {
        log("Loading %.50s\u2026", src);
    }

    if (!strncasecmp(src, prefixes.data.data(), prefixes.data.size())) {
        for (char c = *src; c; c = *(++src)) {
            if (c != ',') continue;

            return decode_base64(++src);
        }

        return std::vector<unsigned char>{};
    }

    if (!strncasecmp(src, prefixes.http.data(),  prefixes.http.size())
    ||  !strncasecmp(src, prefixes.https.data(), prefixes.https.size())) {
        // Download the file from the Internet.
        std::vector<unsigned char> file;

        return file;
    }

    std::filesystem::path path(directory / src);

    if (!std::filesystem::exists(path)) {
        return {};
    }

    // Load the file from the local storage.
    std::ifstream input(path.string(), std::ios::binary);
    int errcode = errno;

    if (!input) {
        log("%s: %s", path.c_str(), strerror(errcode));
        return {};
    }

    std::stringstream sstr;

    input >> sstr.rdbuf();

    const std::string_view view{sstr.view()};

    return std::vector<unsigned char>(&view.front(), &view.back());
}

std::vector<unsigned char> MDMA::dump_image(const Imlib_Image &image) const {
    imlib_context_set_image(image);

    const char *filename = imlib_image_get_filename();

    if (!filename) filename = "memimg";

    int fd, errcode_1{
        (
            fd = memfd_create(filename, MFD_ALLOW_SEALING)
        ) == -1 ? errno : 0
    };

    if (fd == -1) {
        log("memfd_create: %s", strerror(errcode_1));
        return {};
    }

    std::vector<unsigned char> rawimg;

    int tmpfd, errcode_2{(tmpfd = dup(fd)) == -1 ? errno : 0};

    if (tmpfd == -1) {
        log("dup(%d): %s", fd, strerror(errcode_2));
    }
    else {
        imlib_save_image_fd(tmpfd, filename);
        lseek(fd, 0, SEEK_SET);

        std::array<unsigned char, 8*1024> readbuf;
        ssize_t nb;

        do {
            nb = read(fd, readbuf.data(), readbuf.size());

            if (nb == -1) {
                log("read(%d): %s", fd, strerror(errno));
                rawimg.clear();

                break;
            }
            else if (nb == 0) {
                break;
            }
            else if (nb > 0) {
                rawimg.insert(
                    rawimg.end(),
                    (const unsigned char *) (readbuf.data()),
                    (const unsigned char *) (readbuf.data() + nb)
                );
            }
            else die();
        } while (nb != 0);
    }

    if (close(fd) == -1) {
        int errcode = errno;
        log("close(%d): %s", fd, strerror(errcode));
    }

    return rawimg;
}

#endif
