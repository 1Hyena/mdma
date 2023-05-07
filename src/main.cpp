// SPDX-License-Identifier: MIT
#include "options.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>

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

    std::cout << markdown_text << "\n";

    return EXIT_SUCCESS;
}
