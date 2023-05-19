# MDMA #########################################################################

In this context MDMA stands for "MarkDown Monolith Assembler". It is a command
line tool for building elegant browser-based books using the Markdown markup
language.

When given a markdown document as an input, this program produces a monolithic
JavaScript-free HTML5 application as an output. For example, from this very
_readme_ file an HTML document would be generated that renders as shown in the
below screenshot.

|                     [![Screenshot][img-demo]][url-demo]                      |
| :--------------------------------------------------------------------------: |
|  Screenshot depicting how this document renders in the Chromium web browser  |

[img-demo]: screenshot.jpg
[url-demo]: https://raw.githack.com/1Hyena/mdma/master/README.html
            "Click to see this document in your web browser"

To see a live demo of this document in the format of an HTML application, click
on the above screenshot and the respective page will open in your web browser.

# Synopsis #####################################################################

```
Usage: mdma [OPTION]... [FILE]
Options:
  -f  --framework     Specify a custom HTML framework file.
      --brief         Print brief messages (default).
      --debug         Print debugging messages.
  -h  --help          Display this usage information.
      --verbose       Print verbose messages.
  -v  --version       Show version information.

```

If the _FILE_ argument is missing, the program will attempt to read a markdown
document from its standard input.

# Dependencies #################################################################

In order to compile this program from its _Makefile_ the following libraries and
their respective header files must be present in the system.

* [md4c](https://github.com/mity/md4c) —
  C Markdown parser

* [tinyxml2](https://github.com/leethomason/tinyxml2) —
  Simple XML parser made for easy integration

* [tidy](https://www.html-tidy.org/) —
  A tool to tidy down HTML code to a clean style
