// SPDX-License-Identifier: MIT
#include "mdma.h"
#include "options.h"
////////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>

bool load_framework(const std::string &path, std::string &dest);
bool load_markdown (const std::string &path, std::string &dest);

void log(const char *text) {
    std::cerr << text << "\n";
}

void print(const char *text) {
    std::cout << text;
}

int main(int argc, char **argv) {
    OPTIONS options(MDMA::CAPTION, MDMA::VERSION, MDMA::AUTHOR);

    if (!options.deserialize(argc, argv, log)) {
        return EXIT_FAILURE;
    }

    if (options.flags.exit) {
        return EXIT_SUCCESS;
    }

    std::string html;
    std::string md;

    if (!load_framework(options.framework, html)
    ||  !load_markdown(options.file, md)) {
        return EXIT_FAILURE;
    }

    MDMA mdma;
    mdma.cfg.minify = options.flags.minify;

    mdma.set_logger(log);

    const std::string *output{
        mdma.assemble(html.data(), html.size(), md.data(), md.size())
    };

    if (!output) {
        return EXIT_FAILURE;
    }

    print(output->c_str());

    return EXIT_SUCCESS;
}

bool load_markdown(const std::string &path, std::string &dest) {
    if (path.empty()) {
        std::ostringstream std_input;
        std_input << std::cin.rdbuf();
        dest.assign(std_input.str());
    }
    else {
        std::ifstream input(path, std::ios::binary);

        if (!input) {
            std::cerr << path << ": " << strerror(errno) << "\n";
            return false;
        }

        std::stringstream sstr;
        input >> sstr.rdbuf();
        dest.assign(sstr.str());
    }

    if (dest.size() > std::numeric_limits<MD_SIZE>::max()) {
        std::cerr << path << ": file size limit exceeded\n";
        return false;
    }

    return true;
}

bool load_framework(const std::string &path, std::string &framework) {
    auto default_framework{std::to_array<unsigned char>({ MDMA_FRAMEWORK })};

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
