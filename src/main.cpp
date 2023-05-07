// SPDX-License-Identifier: MIT
#include "options.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>
#include <md4c-html.h>

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

    std::string markdown_text;

    if (options.file.empty()) {
        std::ostringstream std_input;
        std_input << std::cin.rdbuf();
        markdown_text.assign(std_input.str());
    }
    else {
        std::ifstream input(options.file);

        if (!input) {
            std::cerr << options.file << ": " << strerror(errno) << "\n";
            return EXIT_FAILURE;
        }

        std::stringstream sstr;
        input >> sstr.rdbuf();
        markdown_text.assign(sstr.str());
    }

    if (markdown_text.size() > std::numeric_limits<MD_SIZE>::max()) {
        std::cerr << options.file << ": file size limit exceeded\n";
        return EXIT_FAILURE;
    }

    fail = md_html(
        markdown_text.c_str(), MD_SIZE(markdown_text.size()), process_output,
        nullptr, MD_DIALECT_GITHUB, 0
    );

    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

void process_output(const MD_CHAR *str, MD_SIZE len, void *userdata) {
    std::string segment(str, len);
    std::cout << segment;
}
