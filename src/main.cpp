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
    const std::string &path, const std::list<tinyxml2::XMLDocument> &sections
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

    if (!parse_framework(options.framework, sections)) {
        return EXIT_FAILURE;
    }
/*
    tinyxml2::XMLPrinter printer;

    for (const tinyxml2::XMLDocument &doc : sections) {
        doc.Print(&printer);
    }

    std::string output(printer.CStr());

    {
        TidyBuffer tidy_buffer{};
        TidyDoc tdoc = tidyCreate();

        tidyOptSetBool(tdoc, TidyIndentContent, yes);
        tidyParseString( tdoc, output.c_str() );
        output.clear();

        tidyCleanAndRepair( tdoc );

        tidySaveBuffer( tdoc, &tidy_buffer );
        output.assign((const char *) tidy_buffer.bp, tidy_buffer.size);

        tidyRelease( tdoc );
        tidyBufFree( &tidy_buffer );
    }

    std::cout << output << "\n";
*/
    for (const auto &p : headings) {
        std::cout << p.first << ": " << p.second << "\n";
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
                    sections.back().NewElement("div")
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

bool parse_framework(
    const std::string &path, const std::list<tinyxml2::XMLDocument> &sections
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

    {
        TidyBuffer tidy_buffer{};
        TidyDoc tdoc = tidyCreate();

        tidyOptSetBool(tdoc, TidyXmlOut, yes);
        tidyOptSetBool(tdoc, TidyHideComments, yes);
        tidyOptSetBool(tdoc, TidyMergeSpans, no);
        tidyOptSetBool(tdoc, TidyDropEmptyElems, no);
        tidyOptSetBool(tdoc, TidyDropEmptyParas, no);

        tidyParseString( tdoc, framework.c_str() );
        framework.clear();

        //tidyCleanAndRepair( tdoc );

        TidyNode root = tidyGetRoot(tdoc);

        while (true) {
            bool repeat = false;

            for (TidyNode child = tidyGetChild(root); child; child = tidyGetNext(child) ) {
                auto node_type = tidyNodeGetType(child);

                if (node_type != TidyNode_Start && node_type != TidyNode_End) {
                    std::cout << "OTHER\n";
                    tidyDiscardElement(tdoc, child);
                    repeat = true;
                    break;
                }

                const char *name = tidyNodeGetName( child );
                if (name) {
                    std::cout << name << "\n";

                    if (!strcasecmp("html", name)) {
                        if (tidyNodeGetText(tdoc, child, &tidy_buffer)) {
                            framework.assign((const char *) tidy_buffer.bp, tidy_buffer.size);
                            break;
                        }
                    }
                }
                else std::cout << "other\n";
            }

            if (!repeat) break;
        }

        //tidySaveBuffer( tdoc, &tidy_buffer );


        tidyRelease( tdoc );
        tidyBufFree( &tidy_buffer );
    }

    //std::cout << framework << "\n";


    tinyxml2::XMLDocument doc;
    doc.Parse(framework.c_str(), framework.size());
    tinyxml2::XMLElement *parent = doc.RootElement();

    if (!parent) {
        std::cerr << path << ": unacceptable content\n";
        return false;
    }

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

            if (id && !strcasecmp("content", id)) {
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

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    std::string output(printer.CStr());

    std::cout << output << "\n";

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
