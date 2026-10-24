#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <filesystem>
#include <thread>
#include <mutex>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t riff_size = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels = 2;
    uint32_t sample_rate = 44100;
    uint32_t byte_rate = 0;
    uint16_t block_align = 0;
    uint16_t bits_per_sample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size = 0;
};
#pragma pack(pop)

struct NcwHeader {
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t sample_rate;
    uint32_t num_samples;
    uint32_t blocks_offset;
    uint32_t data_offset;
    uint32_t data_size;
};

struct BlockHeader {
    uint32_t magic;
    int32_t base_value;
    int16_t bits;
    uint16_t flags;
    uint32_t reserved;
};

static int parseNote(const std::string& str) {
    if (str.empty()) return -1;
    bool allDigits = true;
    for (char c : str) {
        if (!std::isdigit(c)) { allDigits = false; break; }
    }
    if (allDigits) {
        try { return std::stoi(str); } catch (...) { return -1; }
    }

    char letter = std::toupper(str[0]);
    int semitone = 0;
    switch (letter) {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: return -1;
    }

    size_t idx = 1;
    if (idx < str.size() && (str[idx] == '#' || str[idx] == 's')) {
        semitone += 1;
        idx++;
    } else if (idx < str.size() && (str[idx] == 'b' || str[idx] == 'f')) {
        semitone -= 1;
        idx++;
    }

    if (idx < str.size()) {
        try {
            int octave = std::stoi(str.substr(idx));
            return (octave + 1) * 12 + semitone;
        } catch (...) {
            return -1;
        }
    }
    return -1;
}

static inline int32_t sign_extend(uint32_t raw, int bits) {
    if (bits <= 0 || bits >= 32) return static_cast<int32_t>(raw);
    int shift = 32 - bits;
    return (static_cast<int32_t>(raw << shift)) >> shift;
}

class BitReader {
    const uint8_t* data;
    size_t size;
    size_t byte_pos = 0;
    uint64_t accumulator = 0;
    int available = 0;
    int bits;
    uint64_t mask;
public:
    BitReader(const uint8_t* d, size_t s, int b) : data(d), size(s), bits(b) {
        mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1);
    }
    bool next(int32_t& out_val) {
        while (available < bits) {
            if (byte_pos >= size) return false;
            accumulator |= (static_cast<uint64_t>(data[byte_pos++])) << available;
            available += 8;
        }
        uint32_t raw = static_cast<uint32_t>(accumulator & mask);
        out_val = sign_extend(raw, bits);
        accumulator >>= bits;
        available -= bits;
        return true;
    }
};

bool decode_ncw(std::istream& in, uint64_t base_offset, std::vector<int16_t>& out_pcm, NcwHeader& hdr) {
    in.seekg(base_offset, std::ios::beg);
    uint8_t hdr_buf[120];
    if (!in.read(reinterpret_cast<char*>(hdr_buf), 120)) return false;
    
    if (hdr_buf[0] != 0x01 || hdr_buf[1] != 0xa8 || hdr_buf[2] != 0x9e || hdr_buf[3] != 0xd6) {
        return false;
    }
    
    hdr.channels = *reinterpret_cast<uint16_t*>(&hdr_buf[8]);
    hdr.bits_per_sample = *reinterpret_cast<uint16_t*>(&hdr_buf[10]);
    hdr.sample_rate = *reinterpret_cast<uint32_t*>(&hdr_buf[12]);
    hdr.num_samples = *reinterpret_cast<uint32_t*>(&hdr_buf[16]);
    hdr.blocks_offset = *reinterpret_cast<uint32_t*>(&hdr_buf[20]);
    hdr.data_offset = *reinterpret_cast<uint32_t*>(&hdr_buf[24]);
    hdr.data_size = *reinterpret_cast<uint32_t*>(&hdr_buf[28]);
    
    size_t num_blocks = (hdr.num_samples + 511) / 512;
    std::vector<uint32_t> block_offsets(num_blocks + 1);
    
    in.seekg(base_offset + hdr.blocks_offset, std::ios::beg);
    in.read(reinterpret_cast<char*>(block_offsets.data()), (num_blocks + 1) * sizeof(uint32_t));
    
    std::vector<int32_t> ch_samples[2];
    ch_samples[0].reserve(512);
    ch_samples[1].reserve(512);
    
    out_pcm.reserve(hdr.num_samples * hdr.channels);
    
    for (size_t b = 0; b < num_blocks; ++b) {
        uint32_t cur_off = block_offsets[b];
        uint32_t next_off = block_offsets[b + 1];
        if (next_off <= cur_off) continue;
        size_t block_data_len = next_off - cur_off;
        
        in.seekg(base_offset + hdr.data_offset + cur_off, std::ios::beg);
        std::vector<uint8_t> block_bytes(block_data_len);
        in.read(reinterpret_cast<char*>(block_bytes.data()), block_data_len);
        
        const uint8_t* ptr = block_bytes.data();
        size_t rem = block_data_len;
        
        bool is_mid_side = false;
        
        for (int ch = 0; ch < hdr.channels; ++ch) {
            ch_samples[ch].clear();
            if (rem < 16) break;
            
            int32_t base_val = *reinterpret_cast<const int32_t*>(ptr + 4);
            int16_t bits_val = *reinterpret_cast<const int16_t*>(ptr + 8);
            uint16_t flags_val = *reinterpret_cast<const uint16_t*>(ptr + 10);
            
            ptr += 16;
            rem -= 16;
            
            if (ch == 0) {
                is_mid_side = (flags_val & 1) != 0;
            }
            
            int bits = std::abs(bits_val);
            size_t payload_bytes = (bits * 512) / 8;
            size_t actual_payload = std::min(payload_bytes, rem);
            
            if (bits_val > 0) {
                BitReader br(ptr, actual_payload, bits);
                int32_t current = base_val;
                int32_t delta = 0;
                while (ch_samples[ch].size() < 512 && br.next(delta)) {
                    ch_samples[ch].push_back(current);
                    current += delta;
                }
            } else if (bits_val < 0) {
                BitReader br(ptr, actual_payload, bits);
                int32_t val = 0;
                while (ch_samples[ch].size() < 512 && br.next(val)) {
                    ch_samples[ch].push_back(val);
                }
            } else {
                size_t bytes_per_sample = hdr.bits_per_sample / 8;
                for (size_t s = 0; s < 512 && rem >= bytes_per_sample; ++s) {
                    int32_t val = 0;
                    if (bytes_per_sample == 2) {
                        val = *reinterpret_cast<const int16_t*>(ptr);
                    } else if (bytes_per_sample == 3) {
                        val = sign_extend(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16), 24);
                    } else if (bytes_per_sample == 4) {
                        val = *reinterpret_cast<const int32_t*>(ptr);
                    }
                    ch_samples[ch].push_back(val);
                    ptr += bytes_per_sample;
                    rem -= bytes_per_sample;
                }
            }
            
            ptr += actual_payload;
            if (rem >= actual_payload) rem -= actual_payload; else rem = 0;
        }
        
        if (is_mid_side && hdr.channels == 2) {
            size_t count = std::min(ch_samples[0].size(), ch_samples[1].size());
            for (size_t i = 0; i < count; ++i) {
                int32_t mid = ch_samples[0][i];
                int32_t side = ch_samples[1][i];
                ch_samples[0][i] = mid + side;
                ch_samples[1][i] = mid - side;
            }
        }
        
        size_t frames_in_block = std::min<size_t>(512, hdr.num_samples - b * 512);
        for (size_t i = 0; i < frames_in_block; ++i) {
            for (int ch = 0; ch < hdr.channels; ++ch) {
                int32_t sample_val = (i < ch_samples[ch].size()) ? ch_samples[ch][i] : 0;
                int16_t s16;
                if (hdr.bits_per_sample == 16) {
                    s16 = static_cast<int16_t>(sample_val);
                } else if (hdr.bits_per_sample == 24) {
                    s16 = static_cast<int16_t>(sample_val >> 8);
                } else {
                    s16 = static_cast<int16_t>(sample_val >> 16);
                }
                out_pcm.push_back(s16);
            }
        }
    }
    
    return true;
}

struct SampleEntry {
    std::string name;
    uint64_t offset;
    std::string instrument;
    int note = -1;
    int velocity = -1;
};

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <nkx_file> <filter_instrument_or_all> <out_dir>\n";
        std::cerr << "Example: " << argv[0] << " UltimatePianosHD_01.nkx NS3_Bright_Grand /Volumes/SSD-EXT/StagePianoStudio/library\n";
        return 1;
    }
    
    std::string nkx_path = argv[1];
    std::string filter_inst = argv[2];
    std::string out_dir = argv[3];
    
    fs::create_directories(out_dir + "/samples");
    fs::create_directories(out_dir + "/instruments");
    
    std::ifstream in(nkx_path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open " << nkx_path << "\n";
        return 1;
    }
    
    in.seekg(14, std::ios::beg);
    uint32_t num_files = 0;
    in.read(reinterpret_cast<char*>(&num_files), 4);
    in.seekg(0x16, std::ios::beg);
    
    std::vector<SampleEntry> entries;
    for (uint32_t i = 0; i < num_files; ++i) {
        uint16_t rec_len = 0;
        in.read(reinterpret_cast<char*>(&rec_len), 2);
        uint32_t offset_enc = 0;
        in.read(reinterpret_cast<char*>(&offset_enc), 4);
        uint16_t type_hint = 0;
        in.read(reinterpret_cast<char*>(&type_hint), 2);
        
        size_t name_len = rec_len - 8;
        std::vector<uint8_t> name_bytes(name_len);
        in.read(reinterpret_cast<char*>(name_bytes.data()), name_len);
        
        std::string name;
        for (size_t c = 0; c < name_len; c += 2) {
            uint16_t ch = name_bytes[c] | (name_bytes[c+1] << 8);
            if (ch == 0) break;
            if (ch < 128) name.push_back(static_cast<char>(ch));
        }
        
        uint64_t offset = (offset_enc ^ 0x1f4e0c8d);
        
        SampleEntry entry;
        entry.name = name;
        entry.offset = offset;
        
        // Parse instrument, note, velocity: NS3_Bright_Grand_021_061.ncw
        std::string base = name;
        size_t dot = base.rfind('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        
        size_t last_us = base.rfind('_');
        if (last_us != std::string::npos) {
            std::string vel_str = base.substr(last_us + 1);
            size_t prev_us = base.rfind('_', last_us - 1);
            if (prev_us != std::string::npos) {
                std::string note_str = base.substr(prev_us + 1, last_us - prev_us - 1);
                entry.instrument = base.substr(0, prev_us);
                entry.note = parseNote(note_str);
                try {
                    entry.velocity = std::stoi(vel_str);
                } catch (...) {
                    entry.velocity = 127;
                }
            }
        }
        
        if (filter_inst == "all" || entry.instrument == filter_inst) {
            entries.push_back(entry);
        }
    }
    
    std::cout << "Found " << entries.size() << " samples matching filter '" << filter_inst << "'\n";
    if (entries.empty()) return 0;
    
    // Group by instrument -> note -> velocities
    std::map<std::string, std::map<int, std::vector<SampleEntry>>> inst_map;
    for (const auto& e : entries) {
        inst_map[e.instrument][e.note].push_back(e);
    }
    
    std::cout << "Starting multi-threaded extraction & decoding to WAV...\n";
    size_t decoded_count = 0;
    
    for (auto& [inst_name, note_map] : inst_map) {
        std::cout << "\nExtracting Instrument: " << inst_name << " (" << note_map.size() << " notes)\n";
        
        // Build JSON representation
        std::stringstream json;
        json << "{\n";
        json << "  \"name\": \"" << inst_name << "\",\n";
        json << "  \"sample_rate\": 44100,\n";
        json << "  \"channels\": 2,\n";
        json << "  \"notes\": {\n";
        
        bool first_note = true;
        for (auto& [note_num, vel_entries] : note_map) {
            std::sort(vel_entries.begin(), vel_entries.end(), [](const SampleEntry& a, const SampleEntry& b) {
                return a.velocity < b.velocity;
            });
            
            if (!first_note) json << ",\n";
            first_note = false;
            
            json << "    \"" << note_num << "\": [\n";
            
            int prev_vel = 0;
            for (size_t v = 0; v < vel_entries.size(); ++v) {
                const auto& e = vel_entries[v];
                int min_vel = prev_vel + 1;
                int max_vel = (v + 1 == vel_entries.size()) ? 127 : e.velocity;
                prev_vel = e.velocity;
                
                std::string wav_name = e.name;
                size_t dot_ncw = wav_name.rfind(".ncw");
                if (dot_ncw != std::string::npos) wav_name.replace(dot_ncw, 4, ".wav");
                
                std::string out_wav_path = out_dir + "/samples/" + wav_name;
                
                // Decode NCW
                std::vector<int16_t> pcm;
                NcwHeader hdr;
                // NCW starts 31 bytes after the entry offset
                if (decode_ncw(in, e.offset + 31, pcm, hdr)) {
                    std::ofstream out(out_wav_path, std::ios::binary);
                    WavHeader wh;
                    wh.num_channels = hdr.channels;
                    wh.sample_rate = 44100;
                    wh.bits_per_sample = 16;
                    wh.block_align = wh.num_channels * (wh.bits_per_sample / 8);
                    wh.byte_rate = wh.sample_rate * wh.block_align;
                    wh.data_size = pcm.size() * sizeof(int16_t);
                    wh.riff_size = sizeof(WavHeader) - 8 + wh.data_size;
                    
                    out.write(reinterpret_cast<const char*>(&wh), sizeof(wh));
                    out.write(reinterpret_cast<const char*>(pcm.data()), wh.data_size);
                    out.close();
                    decoded_count++;
                    if (decoded_count % 10 == 0) {
                        std::cout << "." << std::flush;
                    }
                }
                
                json << "      {\n";
                json << "        \"min_velocity\": " << min_vel << ",\n";
                json << "        \"max_velocity\": " << max_vel << ",\n";
                json << "        \"target_velocity\": " << e.velocity << ",\n";
                json << "        \"file\": \"" << wav_name << "\"\n";
                json << "      }" << (v + 1 == vel_entries.size() ? "" : ",");
                json << "\n";
            }
            json << "    ]";
        }
        json << "\n  }\n}\n";
        
        std::string json_path = out_dir + "/instruments/" + inst_name + ".json";
        std::ofstream json_file(json_path);
        json_file << json.str();
        json_file.close();
        std::cout << "\nSaved instrument definition: " << json_path << "\n";
    }
    
    std::cout << "\nDone! Successfully extracted and decoded " << decoded_count << " WAV samples.\n";
    return 0;
}
