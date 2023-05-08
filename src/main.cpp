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

void process_output(const MD_CHAR *, MD_SIZE, void *);

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
        std::ifstream input(options.file);

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

    std::string html_text;

    fail = md_html(
        md_text.c_str(), MD_SIZE(md_text.size()), process_output,
        &html_text, MD_DIALECT_GITHUB, MD_HTML_FLAG_XHTML
    );

    {
        TidyBuffer tidy_buffer{};
        TidyDoc tdoc = tidyCreate();

        //tidyOptSetBool(tdoc, TidyXmlTags, yes);
        //tidyOptSetBool(tdoc, TidyHtmlOut, yes);
        //tidyOptSetBool(tdoc, TidyXmlTags, no);
        //tidyOptSetBool(tdoc, TidyXmlOut, no);
        tidyParseString( tdoc, html_text.c_str() );
        html_text.clear();

        //tidyCleanAndRepair( tdoc );

        tidySaveBuffer( tdoc, &tidy_buffer );
        html_text.assign((const char *) tidy_buffer.bp, tidy_buffer.size);

        tidyRelease( tdoc );
        tidyBufFree( &tidy_buffer );

        /*
        TidyNode parent = tidyGetRoot(tdoc);
        int depth = 0;

        while (parent) {
            const char *name = tidyNodeGetName(parent);

            if (name) {
                std::cout << std::string(2*depth, ' ') << "[" << name << "]\n";
            }

            if (tidyNodeGetId(parent) == TidyTag_H1) {
                std::cout << "BOOM\n";
            }

            TidyNode child = tidyGetChild(parent);

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
        */

        /*
        TidyNode body = tidyGetBody(tdoc);

        if (body) {
            TidyNode sibling = tidyGetChild(body);

            std::vector<std::string> tabs;

            for (; sibling; sibling = tidyGetNext(sibling)) {
                if (tidyNodeIsHeader(sibling)) {
                    if (tidyNodeGetId(sibling) == TidyTag_H1) {
                        tabs.emplace_back();
                    }

                    TidyNode child = tidyGetChild(sibling);

                    if (tidyNodeHasText(tdoc, child)) {
                        tidyBufClear(&tidy_buffer);
                        tidyNodeGetValue(tdoc, child, &tidy_buffer);

                        tabs.back().append("<!--").append(
                            (const char *) tidy_buffer.bp, tidy_buffer.size
                        ).append("-->\n");
                    }

                    continue;
                }

                if (tabs.empty()) continue;

                tidyBufClear(&tidy_buffer);
                tidyNodeGetText(tdoc, sibling, &tidy_buffer);

                tabs.back().append(
                    (const char *) tidy_buffer.bp, tidy_buffer.size
                );
            }

            for (size_t i=0; i<tabs.size(); ++i) {
                std::cout << "NEW TAB:\n" << tabs[i] << "\n";
            }
        }

        */
    }

    std::cout << html_text << "\n";

    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

void process_output(const MD_CHAR *str, MD_SIZE len, void *userdata) {
    std::string *html_text = static_cast<std::string *>(userdata);
    html_text->append(str, len);
}
