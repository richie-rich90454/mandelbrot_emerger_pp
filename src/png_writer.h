#ifndef PNG_WRITER_H
#define PNG_WRITER_H
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>
class PixelBuffer;
class PngWriter{
public:
    PngWriter();
    ~PngWriter();
    bool save(const PixelBuffer& buffer, const std::string& path) const;
private:
    void writeChunk(std::ofstream& stream, const char* type, const std::vector<unsigned char>& data) const;
    static void pushU32(std::vector<unsigned char>& out, unsigned int value);
    static unsigned int crc32(const unsigned char* data, std::size_t length);
    static unsigned int adler32(const unsigned char* data, std::size_t length);
};
#endif
