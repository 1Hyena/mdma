// SPDX-License-Identifier: MIT
#ifndef OPTIONS_H_07_05_2023
#define OPTIONS_H_07_05_2023

#include <getopt.h>
#include <string>
#include <functional>
#include <cstring>
#include <cstdarg>

class OPTIONS {
    public:
    static constexpr const char *usage_format{
        "Usage: %s [OPTION]... [FILE]\n"
        "General options:\n"
        "      --brief         Print brief messages (default).\n"
        "      --debug         Print debugging messages.\n"
        "  -f  --framework     Use a custom HTML framework file.\n"
        "  -h  --help          Display this usage information.\n"
        "      --minify        Disable HTML indentation and wrapping.\n"
        "      --monolith      Embed images and styles within the output.\n"
        "  -o  --output        Specify the output file (standard output).\n"
        "  -p  --preview       Set the image preview shrinking factor (%d).\n"
        "      --verbose       Print verbose messages.\n"
        "  -v  --version       Show version information.\n"
        "\n"
        "Markdown dialect options:\n"
        "      --commonmark    Use the CommonMark syntax.\n"
        "      --github        Use Github flavored markdown (default).\n"
    };

    static constexpr const int
        DIALECT_COMMONMARK = 0,
        DIALECT_GITHUB     = 1;

    struct flagset_type {
        int verbose;
        int debug;
        int minify;
        int monolith;
        int dialect;
        int exit;
    };

    OPTIONS(const char *caption, const char *version, const char *copyright)
        : flags(
            {
                .verbose  = 0,
                .debug    = 0,
                .minify   = 0,
                .monolith = 0,
                .dialect  = DIALECT_GITHUB,
                .exit     = 0
            }
        )
        , file         (        "" )
        , framework    (        "" )
        , output       (        "" )
        , preview      (         8 )
        , caption      (   caption )
        , version      (   version )
        , copyright    ( copyright )
        , log_callback (   nullptr ) {}

    OPTIONS() = delete;
    ~OPTIONS() {}

    flagset_type flags;
    std::string  file;
    std::string  framework;
    std::string  output;
    uint8_t      preview;

    std::string caption;
    std::string version;
    std::string copyright;

    inline bool deserialize(
        int argc, char **argv,
        const std::function<void(const char *text)>& log_callback = nullptr
    ) {
        static struct option long_options[] = {
            // These options set a flag:
            { "debug",      no_argument, &flags.debug,                    1 },
            { "brief",      no_argument, &flags.verbose,                  0 },
            { "verbose",    no_argument, &flags.verbose,                  1 },
            { "minify",     no_argument, &flags.minify,                   1 },
            { "monolith",   no_argument, &flags.monolith,                 1 },
            { "commonmark", no_argument, &flags.dialect, DIALECT_COMMONMARK },
            { "github",     no_argument, &flags.dialect,     DIALECT_GITHUB },

            // These options don't set a flag. We distinguish them by indices:
            { "framework",   required_argument, 0, 'f'},
            { "output",      required_argument, 0, 'o'},
            { "preview",     required_argument, 0, 'p'},
            { "help",        no_argument,       0, 'h'},
            { "version",     no_argument,       0, 'v'},
            { 0,             0,                 0,  0 }
        };

        this->log_callback = log_callback;

        while (1) {
            // getopt_long stores the option index here.
            int option_index = -1;

            int c = getopt_long(
                argc, argv, "f:o:p:hv", long_options, &option_index
            );

            // Detect the end of the options.
            if (c == -1) break;

            if (c && option_index == -1) {
                for (const auto &opt : long_options) {
                    if (c != opt.val) {
                        continue;
                    }

                    option_index = static_cast<int>(&opt - &long_options[0]);
                    break;
                }

                if (option_index == -1) {
                    return false;
                }
            }

            switch (c) {
                case 0:
                    {
                        // If this option sets a flag do nothing else.
                        if (long_options[option_index].flag != 0) break;

                        std::string buf="option ";
                        buf.append(long_options[option_index].name);

                        if (optarg) {
                            buf.append(" with arg ");
                            buf.append(optarg);
                        }

                        log("%s", buf.c_str());
                        break;
                    }
                case 'f': {
                    framework.assign(optarg);
                    break;
                }
                case 'h': {
                    fprintf(stdout, usage_format, argv[0], int(preview));
                    flags.exit = 1;

                    break;
                }
                case 'o': {
                    output.assign(optarg);
                    break;
                }
                case 'p': {
                    int i = atoi(optarg);

                    if ((i == 0 && (optarg[0] != '0' || optarg[1] != '\0'))
                    ||  (i < 0 || i > std::numeric_limits<uint8_t>::max())) {
                        log(
                            "invalid %s: %s",
                            long_options[option_index].name, optarg
                        );
                    }
                    else preview = uint8_t(i);

                    break;
                }
                case 'v': {
                    const char *compile_date = __DATE__;
                    const char *compile_year = "unknown year";
                    size_t compile_date_len = strlen(compile_date);

                    while (compile_date_len) {
                        if (compile_date[--compile_date_len] == ' ') {
                            compile_year = &(compile_date[compile_date_len+1]);
                            break;
                        }
                    }

                    printf(
                        "%s %s Copyright (C) %s %s\n",
                        caption.c_str(), version.c_str(), compile_year,
                        copyright.c_str()
                    );

                    flags.exit = 1;
                    break;
                }
                case '?':
                    // getopt_long already printed an error message.
                    break;
                default: return false;
            }
        }

        if (flags.exit) return true;

        if (optind < argc) {
            file.assign(argv[optind++]);

            if (file.empty()) {
                log("%s", "Empty string is not a valid file name.");
                return false;
            }
        }

        while (optind < argc) log("Unidentified argument: %s", argv[optind++]);

        return true;
    }

    void log(const char *fmt, ...) const __attribute__((format(printf, 2, 3))) {
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

    private:
    std::function<void(const char *text)> log_callback;
};

#endif
