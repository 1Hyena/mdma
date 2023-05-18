# MDMA #########################################################################

In this context MDMA stands for "MarkDown Monolith Assembler". It is a command
line tool for building elegant browser-based books using the Markdown markup
language.

When given a markdown document as an input, this program produces a monolithic
JavaScript-free HTML5 application as an output. For example, from this very
_readme_ file an HTML document would be generated that renders as shown in the
below screenshot.

|  ![Screenshot](screenshot.jpg "Screenshot depicting a rendered HTML file")   |
| :--------------------------------------------------------------------------: |
|  Screenshot depicting how this document renders in the Chromium web browser  |

# Synopsis #####################################################################

If the _FILE_ argument is missing, the program will attempt to read a markdown
document from its standard input.

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
