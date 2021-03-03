#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

class SamAlignment {
public:
    std::string qname; // Query template NAME
    uint16_t flag; // bitwise FLAG
    std::string rname; // Reference sequence NAME
    uint32_t pos; // 1-based leftmost mapping POSition
    uint8_t mapq; // MAPping Quality
    std::string cigar; // CIGAR string
    std::string rnext; // Reference name of the mate/NEXT read
    uint32_t pnext; // Position of the mate/NEXT read
    int64_t tlen; // observed Template LENgth
    std::string seq; // segment SEQuence
    std::string qual; // QUALity scores
    std::vector<std::string> opt; // OPTional fields
};

std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(s);
    while (std::getline(token_stream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

class SamParser {
public:
    SamParser(const std::string& file_name): ifs_(file_name), header_() {
        if (!ifs_.is_open()) {
            throw std::runtime_error("Failed to open file: " + file_name);
        }
        ReadHeader();
    }

    std::vector<std::string> Header() {
        return this->header_;
    }

    std::optional<SamAlignment> NextAlignment() {
        std::string line;

        if (std::getline(this->ifs_, line)) {
            std::vector<std::string> fields = Split(line, '\t');
            // for (const auto& f: fields) {
            //     std::cout << f << std::endl;
            // }

            SamAlignment sam_alignment;
            sam_alignment.qname = fields[0];
            std::istringstream(fields[1]) >> sam_alignment.flag;
            sam_alignment.rname = fields[2];
            std::istringstream(fields[3]) >> sam_alignment.pos;
            std::istringstream(fields[4]) >> sam_alignment.mapq;
            sam_alignment.cigar = fields[5];
            sam_alignment.rnext = fields[6];
            std::istringstream(fields[7]) >> sam_alignment.pnext;
            std::istringstream(fields[8]) >> sam_alignment.tlen;
            sam_alignment.seq = fields[9];
            sam_alignment.qual = fields[10];
            if (fields.size() > 10) {
                for (size_t i = 11; i < fields.size(); i++) {
                    sam_alignment.opt.push_back(fields[i]);
                }
            }

            return sam_alignment;
        }

        return std::nullopt;
    }

private:
    void ReadHeader() {
        while (true) {
            std::streampos last_pos = this->ifs_.tellg();
            std::string line;
            if (std::getline(this->ifs_, line)) {
                if (line[0] == '@') {
                    this->header_.push_back(line);
                } else {
                    this->ifs_.seekg(last_pos);
                    break;
                }
            }
        }
    }

private:
    std::ifstream ifs_;
    std::vector<std::string> header_;
};
