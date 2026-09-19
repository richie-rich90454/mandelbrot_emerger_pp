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
const char* GrayscaleScheme::name() const{
    return "grayscale";
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
const char* ThermalScheme::name() const{
    return "thermal";
}
AlphaScheme::AlphaScheme():ColorScheme(){
}
AlphaScheme::~AlphaScheme(){
}
void AlphaScheme::shade(double brightness, Rgba& out) const{
    unsigned char level=clampChannel(std::sqrt(brightness)*std::sqrt(255.0));
    out.r=level;
    out.g=level;
    out.b=level;
    out.a=255;
}
const char* AlphaScheme::name() const{
    return "alpha";
}
// classic Bernstein-polynomial fractal palette: deep blue through cyan, white band and warm rim
RainbowScheme::RainbowScheme():ColorScheme(){
}
RainbowScheme::~RainbowScheme(){
}
void RainbowScheme::shade(double brightness, Rgba& out) const{
    double t=brightness/255.0;
    double omt=1.0-t;
    out.r=clampChannel(9.0*omt*t*t*t*255.0);
    out.g=clampChannel(15.0*omt*omt*t*t*255.0);
    out.b=clampChannel(8.5*omt*omt*omt*t*255.0);
    out.a=255;
}
const char* RainbowScheme::name() const{
    return "rainbow";
}
FireScheme::FireScheme():ColorScheme(){
}
FireScheme::~FireScheme(){
}
void FireScheme::shade(double brightness, Rgba& out) const{
    double t=brightness/255.0;
    out.r=clampChannel(std::fmin(1.0, t*2.0)*255.0);
    out.g=clampChannel(std::fmax(0.0, (t-0.5)*1.9)*255.0);
    out.b=clampChannel(std::fmax(0.0, (t-0.85)/0.15)*255.0);
    out.a=255;
}
const char* FireScheme::name() const{
    return "fire";
}
IceScheme::IceScheme():ColorScheme(){
}
IceScheme::~IceScheme(){
}
void IceScheme::shade(double brightness, Rgba& out) const{
    double t=brightness/255.0;
    out.r=clampChannel(std::fmax(0.0, (t-0.72)*3.5)*255.0);
    out.g=clampChannel(std::fmax(0.0, (t-0.35)*1.7)*255.0);
    out.b=clampChannel(std::fmin(1.0, t*1.9)*255.0);
    out.a=255;
}
const char* IceScheme::name() const{
    return "ice";
}
