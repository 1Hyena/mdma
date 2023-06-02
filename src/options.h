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
        "Options:\n"
        "      --brief         Print brief messages (default).\n"
        "      --debug         Print debugging messages.\n"
        "  -f  --framework     Use a custom HTML framework file.\n"
        "  -h  --help          Display this usage information.\n"
        "      --minify        Disable HTML indentation and wrapping.\n"
        "      --verbose       Print verbose messages.\n"
        "  -v  --version       Show version information.\n"
    };

    struct flagset_type {
        int verbose;
        int debug;
        int minify;
        int exit;
    };

    OPTIONS(const char *caption, const char *version, const char *copyright)
        : flags(
            {
                .verbose = 0,
                .debug   = 0,
                .minify  = 0,
                .exit    = 0
            }
        )
        , file         (        "" )
        , framework    (        "" )
        , caption      (   caption )
        , version      (   version )
        , copyright    ( copyright )
        , log_callback (   nullptr ) {}

    OPTIONS() = delete;
    ~OPTIONS() {}

    flagset_type flags;
    std::string  file;
    std::string  framework;

    std::string caption;
    std::string version;
    std::string copyright;

    inline bool deserialize(
        int argc, char **argv,
        const std::function<void(const char *text)>& log_callback = nullptr
    ) {
        static struct option long_options[] = {
            // These options set a flag:
            { "debug",       no_argument,       &flags.debug,   1 },
            { "brief",       no_argument,       &flags.verbose, 0 },
            { "verbose",     no_argument,       &flags.verbose, 1 },
            { "minify",      no_argument,       &flags.minify,  1 },
            // These options don't set a flag. We distinguish them by indices:
            { "framework",   required_argument, 0,             'f'},
            { "help",        no_argument,       0,             'h'},
            { "version",     no_argument,       0,             'v'},
            { 0,             0,                 0,              0 }
        };

        this->log_callback = log_callback;

        while (1) {
            // getopt_long stores the option index here.
            int option_index = 0;

            int c = getopt_long(
                argc, argv, "f:hv", long_options, &option_index
            );

            // Detect the end of the options.
            if (c == -1) break;

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
                    fprintf(stdout, usage_format, argv[0]);
                    flags.exit = 1;

                    break;
                }
                case 'v': {
                    const char *compile_date = __DATE__;
                    const char *compile_year = "2017";
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
