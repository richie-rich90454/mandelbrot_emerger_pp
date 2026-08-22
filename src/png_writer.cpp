#include "png_writer.h"
#include "pixel_buffer.h"
#include <cstring>
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
unsigned int PngWriter::crc32(const unsigned char* data, std::size_t length){
    unsigned int crc=0xFFFFFFFFu;
    for(std::size_t i=0; i<length; i++){
        crc^=data[i];
        for(int bit=0; bit<8; bit++){
            unsigned int mask=(crc&1u)?0xEDB88320u:0u;
            crc=(crc>>1)^mask;
        }
    }
    return crc^0xFFFFFFFFu;
}
unsigned int PngWriter::adler32(const unsigned char* data, std::size_t length){
    unsigned int a=1u;
    unsigned int b=0u;
    for(std::size_t i=0; i<length; i++){
        a=(a+data[i])%65521u;
        b=(b+a)%65521u;
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
    std::vector<unsigned char> tail(type, type+4);
    if(!data.empty()){
        tail.insert(tail.end(), data.begin(), data.end());
    }
    const unsigned int checksum=crc32(&tail[0], tail.size());
    unsigned char crcBytes[4];
    crcBytes[0]=static_cast<unsigned char>((checksum>>24)&0xFFu);
    crcBytes[1]=static_cast<unsigned char>((checksum>>16)&0xFFu);
    crcBytes[2]=static_cast<unsigned char>((checksum>>8)&0xFFu);
    crcBytes[3]=static_cast<unsigned char>(checksum&0xFFu);
    stream.write(reinterpret_cast<const char*>(crcBytes), 4);
}
bool PngWriter::save(const PixelBuffer& buffer, const std::string& path) const{
    const int width=buffer.getWidth();
    const int height=buffer.getHeight();
    const unsigned char* pixels=buffer.data();
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
    idat.push_back(0x78);
    idat.push_back(0x01);
    std::size_t offset=0;
    while(offset<rawSize){
        std::size_t blockSize=rawSize-offset;
        if(blockSize>65535u){
            blockSize=65535u;
        }
        bool finalBlock=(offset+blockSize==rawSize);
        idat.push_back(finalBlock?1u:0u);
        idat.push_back(static_cast<unsigned char>(blockSize&0xFFu));
        idat.push_back(static_cast<unsigned char>((blockSize>>8)&0xFFu));
        idat.push_back(static_cast<unsigned char>((~blockSize)&0xFFu));
        idat.push_back(static_cast<unsigned char>(((~blockSize)>>8)&0xFFu));
        for(std::size_t i=0; i<blockSize; i++){
            idat.push_back(raw[offset+i]);
        }
        offset+=blockSize;
        if(blockSize==0){
            break;
        }
    }
    pushU32(idat, adler32(&raw[0], rawSize));
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
