#include <fstream>
#include <vector>

class SamRecord {
    std::string qname;  // Query template NAME
    uint16_t    flag;   // bitwise FLAG (uint16_t)
    std::string rname;  // Reference sequence NAME
    uint32_t    pos;    // 1-based leftmost mapping POSition (uint32_t)
    uint8_t     mapq;   // MAPping Quality (uint8_t)
    std::string cigar;  // CIGAR string
    std::string rnext;  // Ref. name of the mate/NEXT read
    uint32_t    pnext;  // Position of the mate/NEXT read (uint32_t)
    int64_t     tlen;   // observed Template LENgth (int64_t)
    std::string seq;    // segment SEQuence
    std::string qual;   // QUALity scores
    std::string opt;    // OPTional information
};

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

    std::string NextAlignment() {
        std::string line;
        std::getline(this->ifs_, line);
        return line;
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



// static void samparser_parse(samparser_t *samparser)
// {
//     size_t l = strlen(samparser->curr.line) - 1;

//     while (l && (samparser->curr.line[l] == '\r'
//                || samparser->curr.line[l] == '\n'))
//         samparser->curr.line[l--] = '\0';

//     char *c = samparser->curr.qname = samparser->curr.line;
//     int f = 1;

//     while (*c) {
//         if (*c == '\t') {
//             if (f ==  1) samparser->curr.flag  = (uint16_t)atoi(c + 1);
//             if (f ==  2) samparser->curr.rname = c + 1;
//             if (f ==  3) samparser->curr.pos   = (uint32_t)atoi(c + 1);
//             if (f ==  4) samparser->curr.mapq  = (uint8_t)atoi(c + 1);
//             if (f ==  5) samparser->curr.cigar = c + 1;
//             if (f ==  6) samparser->curr.rnext = c + 1;
//             if (f ==  7) samparser->curr.pnext = (uint32_t)atoi(c + 1);
//             if (f ==  8) samparser->curr.tlen  = (int64_t)atoi(c + 1);
//             if (f ==  9) samparser->curr.seq   = c + 1;
//             if (f == 10) samparser->curr.qual  = c + 1;
//             if (f == 11) samparser->curr.opt   = c + 1;
//             f++;
//             *c = '\0';
//             if (f == 12) break;
//         }
//         c++;
//     }

//     if (f == 11) samparser->curr.opt = c;
// }

