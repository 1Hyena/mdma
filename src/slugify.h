// SPDX-License-Identifier: MIT
/*
MIT License

Copyright (c) 2017 Thomas Brüggemann
Copyright (c) 2023 Erich Erstu

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef SLUGIFY_H_06_06_2023
#define SLUGIFY_H_06_06_2023

#include <string>
#include <unordered_map>
#include <regex>
#include <algorithm>

std::string slugify(std::string input) {
    std::unordered_map<std::string, std::string> charMap {
        // latin
        {"À", "A"}, {"Á", "A"}, {"Â", "A"}, {"Ã", "A"}, {"Ä", "A"}, {"Å", "A"},
        {"Æ", "AE"}, {"Ç", "C"}, {"È", "E"}, {"É", "E"}, {"Ê", "E"}, {"Ë", "E"},
        {"Ì", "I"}, {"Í", "I"}, {"Î", "I"}, {"Ï", "I"}, {"Ð", "D"}, {"Ñ", "N"},
        {"Ò", "O"}, {"Ó", "O"}, {"Ô", "O"}, {"Õ", "O"}, {"Ö", "O"}, {"Ő", "O"},
        {"Ø", "O"}, {"Ù", "U"}, {"Ú", "U"}, {"Û", "U"}, {"Ü", "U"}, {"Ű", "U"},
        {"Ý", "Y"}, {"Þ", "TH"}, {"ß", "ss"}, {"à", "a"}, {"á", "a"},
        {"â", "a"}, {"ã", "a"}, {"ä", "a"}, {"å", "a"}, {"æ", "ae"}, {"ç", "c"},
        {"è", "e"}, {"é", "e"}, {"ê", "e"}, {"ë", "e"}, {"ì", "i"}, {"í", "i"},
        {"î", "i"}, {"ï", "i"}, {"ð", "d"}, {"ñ", "n"}, {"ò", "o"}, {"ó", "o"},
        {"ô", "o"}, {"õ", "o"}, {"ö", "o"}, {"ő", "o"}, {"ø", "o"}, {"ù", "u"},
        {"ú", "u"}, {"û", "u"}, {"ü", "u"}, {"ű", "u"}, {"ý", "y"}, {"þ", "th"},
        {"ÿ", "y"}, {"ẞ", "SS"},
        // greek
        {"α", "a"}, {"β", "b"}, {"γ", "g"}, {"δ", "d"}, {"ε", "e"}, {"ζ", "z"},
        {"η", "h"}, {"θ", "8"}, {"ι", "i"}, {"κ", "k"}, {"λ", "l"}, {"μ", "m"},
        {"ν", "n"}, {"ξ", "3"}, {"ο", "o"}, {"π", "p"}, {"ρ", "r"}, {"σ", "s"},
        {"τ", "t"}, {"υ", "y"}, {"φ", "f"}, {"χ", "x"}, {"ψ", "ps"}, {"ω", "w"},
        {"ά", "a"}, {"έ", "e"}, {"ί", "i"}, {"ό", "o"}, {"ύ", "y"}, {"ή", "h"},
        {"ώ", "w"}, {"ς", "s"}, {"ϊ", "i"}, {"ΰ", "y"}, {"ϋ", "y"}, {"ΐ", "i"},
        {"Α", "A"}, {"Β", "B"}, {"Γ", "G"}, {"Δ", "D"}, {"Ε", "E"}, {"Ζ", "Z"},
        {"Η", "H"}, {"Θ", "8"}, {"Ι", "I"}, {"Κ", "K"}, {"Λ", "L"}, {"Μ", "M"},
        {"Ν", "N"}, {"Ξ", "3"}, {"Ο", "O"}, {"Π", "P"}, {"Ρ", "R"}, {"Σ", "S"},
        {"Τ", "T"}, {"Υ", "Y"}, {"Φ", "F"}, {"Χ", "X"}, {"Ψ", "PS"}, {"Ω", "W"},
        {"Ά", "A"}, {"Έ", "E"}, {"Ί", "I"}, {"Ό", "O"}, {"Ύ", "Y"}, {"Ή", "H"},
        {"Ώ", "W"}, {"Ϊ", "I"}, {"Ϋ", "Y"},
        // turkish
        {"ş", "s"}, {"Ş", "S"}, {"ı", "i"}, {"İ", "I"}, {"ç", "c"}, {"Ç", "C"},
        {"ü", "u"}, {"Ü", "U"}, {"ö", "o"}, {"Ö", "O"}, {"ğ", "g"}, {"Ğ", "G"},
        // russian
        {"а", "a"}, {"б", "b"}, {"в", "v"}, {"г", "g"}, {"д", "d"}, {"е", "e"},
        {"ё", "yo"}, {"ж", "zh"}, {"з", "z"}, {"и", "i"}, {"й", "j"},
        {"к", "k"}, {"л", "l"}, {"м", "m"}, {"н", "n"}, {"о", "o"}, {"п", "p"},
        {"р", "r"}, {"с", "s"}, {"т", "t"}, {"у", "u"}, {"ф", "f"}, {"х", "h"},
        {"ц", "c"}, {"ч", "ch"}, {"ш", "sh"}, {"щ", "sh"}, {"ъ", "u"},
        {"ы", "y"}, {"ь", ""}, {"э", "e"}, {"ю", "yu"}, {"я", "ya"}, {"А", "A"},
        {"Б", "B"}, {"В", "V"}, {"Г", "G"}, {"Д", "D"}, {"Е", "E"}, {"Ё", "Yo"},
        {"Ж", "Zh"}, {"З", "Z"}, {"И", "I"}, {"Й", "J"}, {"К", "K"}, {"Л", "L"},
        {"М", "M"}, {"Н", "N"}, {"О", "O"}, {"П", "P"}, {"Р", "R"}, {"С", "S"},
        {"Т", "T"}, {"У", "U"}, {"Ф", "F"}, {"Х", "H"}, {"Ц", "C"}, {"Ч", "Ch"},
        {"Ш", "Sh"}, {"Щ", "Sh"}, {"Ъ", "U"}, {"Ы", "Y"}, {"Ь", ""}, {"Э", "E"},
        {"Ю", "Yu"}, {"Я", "Ya"},
        // ukranian
        {"Є", "Ye"}, {"І", "I"}, {"Ї", "Yi"}, {"Ґ", "G"}, {"є", "ye"},
        {"і", "i"}, {"ї", "yi"}, {"ґ", "g"},
        // czech
        {"č", "c"}, {"ď", "d"}, {"ě", "e"}, {"ň", "n"}, {"ř", "r"}, {"š", "s"},
        {"ť", "t"}, {"ů", "u"}, {"ž", "z"}, {"Č", "C"}, {"Ď", "D"}, {"Ě", "E"},
        {"Ň", "N"}, {"Ř", "R"}, {"Š", "S"}, {"Ť", "T"}, {"Ů", "U"}, {"Ž", "Z"},
        // polish
        {"ą", "a"}, {"ć", "c"}, {"ę", "e"}, {"ł", "l"}, {"ń", "n"}, {"ó", "o"},
        {"ś", "s"}, {"ź", "z"}, {"ż", "z"}, {"Ą", "A"}, {"Ć", "C"}, {"Ę", "e"},
        {"Ł", "L"}, {"Ń", "N"}, {"Ś", "S"}, {"Ź", "Z"}, {"Ż", "Z"},
        // latvian
        {"ā", "a"}, {"č", "c"}, {"ē", "e"}, {"ģ", "g"}, {"ī", "i"}, {"ķ", "k"},
        {"ļ", "l"}, {"ņ", "n"}, {"š", "s"}, {"ū", "u"}, {"ž", "z"}, {"Ā", "A"},
        {"Č", "C"}, {"Ē", "E"}, {"Ģ", "G"}, {"Ī", "i"}, {"Ķ", "k"}, {"Ļ", "L"},
        {"Ņ", "N"}, {"Š", "S"}, {"Ū", "u"}, {"Ž", "Z"},
        // currency
        {"€", "euro"}, {"₢", "cruzeiro"}, {"₣", "french franc"}, {"£", "pound"},
        {"₤", "lira"}, {"₥", "mill"}, {"₦", "naira"}, {"₧", "peseta"},
        {"₨", "rupee"}, {"₩", "won"}, {"₪", "new shequel"}, {"₫", "dong"},
        {"₭", "kip"}, {"₮", "tugrik"}, {"₯", "drachma"}, {"₰", "penny"},
        {"₱", "peso"}, {"₲", "guarani"}, {"₳", "austral"}, {"₴", "hryvnia"},
        {"₵", "cedi"}, {"¢", "cent"}, {"¥", "yen"}, {"元", "yuan"},
        {"円", "yen"}, {"﷼", "rial"}, {"₠", "ecu"}, {"¤", "currency"},
        {"฿", "baht"}, {"$", "dollar"},
        // symbols
        {"©", "(c)"}, {"œ", "oe"}, {"Œ", "OE"}, {"∑", "sum"}, {"®", "(r)"},
        {"†", "+"}, {"“", "\""}, {"∂", "d"}, {"ƒ", "f"}, {"™", "tm"},
        {"℠", "sm"}, {"…", "..."}, {"˚", "o"}, {"º", "o"}, {"ª", "a"},
        {"•", "*"}, {"∆", "delta"}, {"∞", "infinity"}, {"♥", "love"},
        {"&", "and"}, {"|", "or"}, {"<", "less"}, {">", "greater"}
    };

    // loop every character in charMap
    for(auto kv : charMap)
    {
	    // check if key is in string
	    if(input.find(kv.first) != std::string::npos)
	    {
		    // replace key with value
		    input.replace(input.find(kv.first), kv.first.length(), kv.second);
	    }
    }

    input = std::regex_replace(
        input, std::regex("[\\s]+"), "-"
    );

    input = std::regex_replace(
        input, std::regex("[^A-Za-z0-9_-]+"), ""
    );

    input = std::regex_replace(
        input, std::regex("^[^A-Za-z]+"), ""
    );

    input = std::regex_replace(
        input, std::regex("[-]+"), "-"
    );

    /*
    std::regex e1("[^\\w\\s$*_+~.()\'\"-]");
    input = std::regex_replace(input, e1, "");

    std::regex e2("^\\s+|\\s+$");
    input = std::regex_replace(input, e2, "");

    std::regex e3("[-\\s]+");
    input = std::regex_replace(input, e3, "-");

    std::regex e4("#-$");
    input = std::regex_replace(input, e4, "");
    */

    return input;
};

#endif
