#include "pixel_buffer.h"
PixelBuffer::PixelBuffer(int width, int height):width(width),height(height),pixels(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*4u, 0){
}
int PixelBuffer::getWidth() const{
    return width;
}
int PixelBuffer::getHeight() const{
    return height;
}
unsigned char* PixelBuffer::data(){
    return &pixels[0];
}
const unsigned char* PixelBuffer::data() const{
    return &pixels[0];
}
