#ifndef PIXEL_BUFFER_H
#define PIXEL_BUFFER_H
#include <cstddef>
#include <vector>
class PixelBuffer{
public:
    PixelBuffer(int width, int height);
    int getWidth() const;
    int getHeight() const;
    unsigned char* data();
    const unsigned char* data() const;
private:
    int width;
    int height;
    std::vector<unsigned char> pixels;
};
#endif
