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
#include <signal.h>

bool parse_framework(const std::string &path);
bool parse_markdown(const std::string &path, std::list<tinyxml2::XMLDocument> &);

int main(int argc, char **argv) {
    OPTIONS options("MarkDown Monolith Assembler", "1.0", "Erich Erstu");

    bool fail = !options.deserialize(
        argc, argv,
        [&](const char *txt){
            std::cerr << txt << "\n";
        }
    );

    if (fail) return EXIT_FAILURE;

    if (options.flags.exit) {
        return EXIT_SUCCESS;
    }

    if (!parse_framework(options.framework)) {
        return EXIT_FAILURE;
    }

    std::list<tinyxml2::XMLDocument> sections;

    if (!parse_markdown(options.file, sections)) {
        return EXIT_FAILURE;
    }

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

    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

const tinyxml2::XMLElement *find_if(
    const tinyxml2::XMLElement &root,
    std::function<bool(const tinyxml2::XMLElement &, int)> fun
);

bool parse_framework(const std::string &path) {
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

    return true;
}

bool parse_markdown(
    const std::string &path, std::list<tinyxml2::XMLDocument> &sections
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
    int next_id=1;

    for (; first_heading; sibling = sibling->NextSiblingElement()) {
        const char *name = sibling ? sibling->Name() : nullptr;

        tinyxml2::XMLElement *root = nullptr;

        if ((name && !strcasecmp("h1", name)) || !sibling) {
            if (!sibling) {
                break;
            }

            sections.emplace_back();

            tinyxml2::XMLNode *node{
                sections.back().InsertFirstChild(
                    sections.back().NewElement("div")
                )
            };

            tinyxml2::XMLElement *elem{
                node ? node->ToElement() : nullptr
            };

            if (elem) {
                elem->SetAttribute("class", "tab");

                node = elem->InsertFirstChild(
                    sections.back().NewElement("a")
                );

                elem = node ? node->ToElement() : nullptr;

                if (elem) {
                    std::string id{"chapter-"};
                    id.append(std::to_string(next_id++));

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
        else {
            root = sections.back().RootElement();
        }

        if (sections.empty()) continue;

        tinyxml2::XMLNode *node = sibling->DeepClone(&sections.back());

        if (node) {
            root->InsertEndChild(node);
        }
    }


    return true;
}

const tinyxml2::XMLElement *find_if(
    const tinyxml2::XMLElement &root,
    std::function<bool(const tinyxml2::XMLElement &, int)> fun
) {
    int depth = 0;

    const tinyxml2::XMLElement *parent = &root;

    while (parent) {
        if (fun(*parent, depth)) {
            return parent;
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
