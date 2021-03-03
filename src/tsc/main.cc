#include <cstdlib>
#include <iostream>
#include <string>

#include "sam.h"

class Options {
public:
    Options() : decompress(false) {}

    void Print() {
        std::cout << "Options:" << std::endl;
        std::cout << "  decompress:      " << (this->decompress ? "true" : "false") << std::endl;
        std::cout << "  input_file_name: " << this->input_file_name << std::endl;
    }

    bool decompress;
    std::string input_file_name;
};

static void PrintHelp(const std::string& prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS] FILE" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h,--help        print this help" << std::endl;
    std::cout << "  -d,--decompress  decompress" << std::endl;
    std::cout << "  -i,--input FILE  input FILE" << std::endl;
}

Options ParseArgs(int argc, char *argv[]) {
    Options opts;
    std::string prog_name = argv[0];
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if ((arg == "-h") || (arg == "--help")) {
            PrintHelp(prog_name);
            throw std::runtime_error("help");
        }

        if ((arg == "-d") || (arg == "--decompress")) {
            opts.decompress = true;
        } else if ((arg == "-i") || (arg == "--input")) {
            if ((i + 1) < argc) {
                opts.input_file_name = argv[i + 1];
                i++;
            } else {
                throw std::runtime_error("Option 'i, --input' requires one argument");
            }
        } else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }
    return opts;
}

int main(int argc, char *argv[]) {
    try {
        Options opts = ParseArgs(argc, argv);
        opts.Print();

        SamParser sam_parser(opts.input_file_name);
        std::cout << "Header: " << std::endl;
        std::vector<std::string> header = sam_parser.Header();
        for (const auto& header_line: header) {
            std::cout << header_line << std::endl;
        }

        while (true) {
            auto alignment = sam_parser.NextAlignment();
            if (!alignment.has_value()) break;
            std::cout << "Alignment: " << alignment.value().qname << std::endl;
        }
    } catch (const std::exception& e) {
        if (std::string(e.what()) == "help") {
            return EXIT_SUCCESS;
        }
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
