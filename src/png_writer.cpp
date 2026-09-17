#include "png_writer.h"
#include <cstring>
namespace{
    struct Crc32Table{
        unsigned int entries[256];
        Crc32Table(){
            for(unsigned int i=0; i<256; i++){
                unsigned int value=i;
                for(int bit=0; bit<8; bit++){
                    value=(value&1u)?((value>>1)^0xEDB88320u):(value>>1);
                }
                entries[i]=value;
            }
        }
    };
    const Crc32Table CRC32_TABLE;
}
PngWriter::PngWriter(){
}
PngWriter::~PngWriter(){
}
void PngWriter::pushU32(std::vector<unsigned char>& out, unsigned int value){
    out.push_back(static_cast<unsigned char>((value>>24)&0xFFu));
    out.push_back(static_cast<unsigned char>((value>>16)&0xFFu));
    out.push_back(static_cast<unsigned char>((value>>8)&0xFFu));
    out.push_back(static_cast<unsigned char>(value&0xFFu));
}
unsigned int PngWriter::crc32Update(unsigned int crc, const unsigned char* data, std::size_t length){
    for(std::size_t i=0; i<length; i++){
        crc=CRC32_TABLE.entries[(crc^data[i])&0xFFu]^(crc>>8);
    }
    return crc;
}
// canonical adler32 chunking: NMAX=5552 keeps a and b inside 32 bits so the modulo runs per chunk
unsigned int PngWriter::adler32(const unsigned char* data, std::size_t length){
    unsigned int a=1u;
    unsigned int b=0u;
    while(length>0){
        std::size_t chunk=(length>5552u)?5552u:length;
        length-=chunk;
        while(chunk>0){
            a+=*data++;
            b+=a;
            chunk--;
        }
        a%=65521u;
        b%=65521u;
    }
    return (b<<16)|a;
}
// PNG with stored (uncompressed) deflate blocks: simple and dependency-free
void PngWriter::writeChunk(std::ofstream& stream, const char* type, const std::vector<unsigned char>& data) const{
    std::vector<unsigned char> header;
    pushU32(header, static_cast<unsigned int>(data.size()));
    for(int i=0; i<4; i++){
        header.push_back(static_cast<unsigned char>(type[i]));
    }
    stream.write(reinterpret_cast<const char*>(&header[0]), static_cast<std::streamsize>(header.size()));
    if(!data.empty()){
        stream.write(reinterpret_cast<const char*>(&data[0]), static_cast<std::streamsize>(data.size()));
    }
    unsigned int checksum=crc32Update(0xFFFFFFFFu, reinterpret_cast<const unsigned char*>(type), 4);
    if(!data.empty()){
        checksum=crc32Update(checksum, &data[0], data.size());
    }
    checksum^=0xFFFFFFFFu;
    unsigned char crcBytes[4];
    crcBytes[0]=static_cast<unsigned char>((checksum>>24)&0xFFu);
    crcBytes[1]=static_cast<unsigned char>((checksum>>16)&0xFFu);
    crcBytes[2]=static_cast<unsigned char>((checksum>>8)&0xFFu);
    crcBytes[3]=static_cast<unsigned char>(checksum&0xFFu);
    stream.write(reinterpret_cast<const char*>(crcBytes), 4);
}
bool PngWriter::save(const unsigned char* pixels, int width, int height, const std::string& path) const{
    std::vector<unsigned char> ihdr;
    pushU32(ihdr, static_cast<unsigned int>(width));
    pushU32(ihdr, static_cast<unsigned int>(height));
    ihdr.push_back(8);
    ihdr.push_back(6);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    const std::size_t rowSize=static_cast<std::size_t>(width)*4u+1u;
    const std::size_t rawSize=rowSize*static_cast<std::size_t>(height);
    std::vector<unsigned char> raw(rawSize, 0);
    for(int y=0; y<height; y++){
        raw[rowSize*static_cast<std::size_t>(y)]=0;
        std::memcpy(&raw[rowSize*static_cast<std::size_t>(y)+1u], pixels+static_cast<std::size_t>(y)*static_cast<std::size_t>(width)*4u, rowSize-1u);
    }
    std::vector<unsigned char> idat;
    idat.reserve(rawSize+(rawSize/65535u+1u)*5u+8u);
    idat.push_back(0x78);
    idat.push_back(0x01);
    std::size_t offset=0;
    while(offset<rawSize){
        std::size_t blockSize=rawSize-offset;
        if(blockSize>65535u){
            blockSize=65535u;
        }
        const bool finalBlock=(offset+blockSize==rawSize);
        idat.push_back(finalBlock?1u:0u);
        idat.push_back(static_cast<unsigned char>(blockSize&0xFFu));
        idat.push_back(static_cast<unsigned char>((blockSize>>8)&0xFFu));
        idat.push_back(static_cast<unsigned char>((~blockSize)&0xFFu));
        idat.push_back(static_cast<unsigned char>(((~blockSize)>>8)&0xFFu));
        idat.insert(idat.end(), raw.begin()+static_cast<std::ptrdiff_t>(offset), raw.begin()+static_cast<std::ptrdiff_t>(offset+blockSize));
        offset+=blockSize;
    }
    pushU32(idat, adler32(raw.data(), rawSize));
    std::ofstream stream(path.c_str(), std::ios::binary);
    if(!stream.is_open()){
        return false;
    }
    const unsigned char signature[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    stream.write(reinterpret_cast<const char*>(signature), 8);
    writeChunk(stream, "IHDR", ihdr);
    writeChunk(stream, "IDAT", idat);
    writeChunk(stream, "IEND", std::vector<unsigned char>());
    stream.close();
    return stream.good();
}
