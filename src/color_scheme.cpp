#include "color_scheme.h"
#include <cmath>
ColorScheme::ColorScheme(){
}
ColorScheme::~ColorScheme(){
}
unsigned char ColorScheme::clampChannel(double value){
    if(!(value>0.0)){
        return 0;
    }
    if(value>255.0){
        return 255;
    }
    return static_cast<unsigned char>(value+0.5);
}
GrayscaleScheme::GrayscaleScheme():ColorScheme(){
}
GrayscaleScheme::~GrayscaleScheme(){
}
void GrayscaleScheme::shade(double brightness, Rgba& out) const{
    unsigned char level=clampChannel(brightness);
    out.r=level;
    out.g=level;
    out.b=level;
    out.a=255;
}
ThermalScheme::ThermalScheme():ColorScheme(){
}
ThermalScheme::~ThermalScheme(){
}
void ThermalScheme::shade(double brightness, Rgba& out) const{
    out.r=clampChannel(brightness*brightness/255.0);
    out.g=clampChannel(brightness*brightness*brightness/65025.0);
    out.b=clampChannel(brightness/1.2);
    out.a=255;
}
AlphaScheme::AlphaScheme():ColorScheme(){
}
AlphaScheme::~AlphaScheme(){
}
void AlphaScheme::shade(double brightness, Rgba& out) const{
    out.r=255;
    out.g=255;
    out.b=255;
    out.a=clampChannel(std::sqrt(brightness)*std::sqrt(255.0));
}
