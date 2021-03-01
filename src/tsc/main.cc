#include <iostream>

class Options {
public:
    Options() : decompress(false) {}

    void Print() {
        std::cout << "Options:" << std::endl;
        std::cout << "  decompress: " << (this->decompress ? "true" : "false") << std::endl;
    }

    bool decompress;
};

static void PrintHelp(const std::string& prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS] FILE" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h,--help        print this help" << std::endl;
    std::cout << "  -d,--decompress  decompress" << std::endl;
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
        }
    }
    return opts;
}

int main(int argc, char *argv[]) {
    try {
        Options opts = ParseArgs(argc, argv);
        opts.Print();
    } catch (const std::exception& e) {
        if (e.what() == "help") {
            return EXIT_SUCCESS;
        }
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
