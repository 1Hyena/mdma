// SPDX-License-Identifier: MIT
#include "options.h"
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

void process_output(const MD_CHAR *, MD_SIZE, void *);

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

    std::string md_text;

    if (options.file.empty()) {
        std::ostringstream std_input;
        std_input << std::cin.rdbuf();
        md_text.assign(std_input.str());
    }
    else {
        std::ifstream input(options.file, std::ios::binary);

        if (!input) {
            std::cerr << options.file << ": " << strerror(errno) << "\n";
            return EXIT_FAILURE;
        }

        std::stringstream sstr;
        input >> sstr.rdbuf();
        md_text.assign(sstr.str());
    }

    if (md_text.size() > std::numeric_limits<MD_SIZE>::max()) {
        std::cerr << options.file << ": file size limit exceeded\n";
        return EXIT_FAILURE;
    }

    std::string xhtml_text;

    fail = md_html(
        md_text.c_str(), MD_SIZE(md_text.size()), process_output,
        &xhtml_text, MD_DIALECT_GITHUB, MD_HTML_FLAG_XHTML
    );

    {
        tinyxml2::XMLDocument doc;

        doc.Parse(xhtml_text.c_str(), xhtml_text.size());

        tinyxml2::XMLElement *parent = doc.RootElement();
        const tinyxml2::XMLElement *first_heading = nullptr;

        std::list<tinyxml2::XMLDocument> chapters;

        tinyxml2::XMLPrinter printer;

        if (parent) {
            first_heading = find_if(
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
                    if (!chapters.empty()) {
                        chapters.back().Print(&printer);
                    }

                    if (!sibling) {
                        break;
                    }

                    chapters.emplace_back();

                    tinyxml2::XMLNode *node{
                        chapters.back().InsertFirstChild(
                            chapters.back().NewElement("div")
                        )
                    };

                    tinyxml2::XMLElement *elem{
                        node ? node->ToElement() : nullptr
                    };

                    if (elem) {
                        elem->SetAttribute("class", "tab");

                        node = elem->InsertFirstChild(
                            chapters.back().NewElement("a")
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
                    root = chapters.back().RootElement();
                }

                if (chapters.empty()) continue;

                tinyxml2::XMLNode *node = sibling->DeepClone(&chapters.back());

                if (node) {
                    root->InsertEndChild(node);
                }
            }
        }

        xhtml_text.assign(printer.CStr());
    }

    {
        TidyBuffer tidy_buffer{};
        TidyDoc tdoc = tidyCreate();

        tidyOptSetBool(tdoc, TidyIndentContent, yes);
        tidyParseString( tdoc, xhtml_text.c_str() );
        xhtml_text.clear();

        tidyCleanAndRepair( tdoc );

        tidySaveBuffer( tdoc, &tidy_buffer );
        xhtml_text.assign((const char *) tidy_buffer.bp, tidy_buffer.size);

        tidyRelease( tdoc );
        tidyBufFree( &tidy_buffer );
    }

    std::cout << xhtml_text << "\n";

    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

void process_output(const MD_CHAR *str, MD_SIZE len, void *userdata) {
    std::string *dest = static_cast<std::string *>(userdata);
    dest->append(str, len);
}
