#include "viewport.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
Viewport::Viewport(int cssWidth, int cssHeight):cssWidth(cssWidth),cssHeight(cssHeight),deviceWidth(cssWidth*2),deviceHeight(cssHeight*2),aspectRatio(static_cast<double>(cssWidth)/static_cast<double>(cssHeight)),boundXi(),boundXf(),boundYi(),boundYf(),selectionPending(false),pendingPlaneX(),pendingPlaneY(){
    initializeBounds();
}
void Viewport::setDeviceSize(int width, int height){
    if(width<=0 || height<=0){
        return;
    }
    deviceWidth=width;
    deviceHeight=height;
}
void Viewport::initializeBounds(){
    const int limbs=Big::limbsForSpan(0.5);
    boundXi=Big::fromDouble(-2.0, limbs);
    boundXf=Big::fromDouble(2.0, limbs);
    boundYi=Big::fromDouble(-2.0*static_cast<double>(cssHeight)/static_cast<double>(cssWidth), limbs);
    boundYf=Big::fromDouble(2.0*static_cast<double>(cssHeight)/static_cast<double>(cssWidth), limbs);
    selectionPending=false;
    pendingPlaneX=Big();
    pendingPlaneY=Big();
}
// every value in the viewport is an exact multiple of the current grid, so padding limbs is lossless;
// keeping one zoom step of headroom means truncation never eats into the next generation's resolution
void Viewport::ensurePrecision(double hintSpan){
    const int limbs=Big::limbsForSpan(hintSpan);
    boundXi=boundXi.withLimbs(limbs);
    boundXf=boundXf.withLimbs(limbs);
    boundYi=boundYi.withLimbs(limbs);
    boundYf=boundYf.withLimbs(limbs);
    pendingPlaneX=pendingPlaneX.withLimbs(limbs);
    pendingPlaneY=pendingPlaneY.withLimbs(limbs);
}
double Viewport::spanX() const{
    return Big::sub(boundXf, boundXi).toDouble();
}
double Viewport::spanY() const{
    return Big::sub(boundYf, boundYi).toDouble();
}
void Viewport::beginZoom(double deviceX, double deviceY){
    ensurePrecision(std::min(spanX(), spanY())/8.0);
    pendingPlaneX=planeFromDeviceX(deviceX);
    pendingPlaneY=planeFromDeviceY(deviceY);
    selectionPending=true;
}
// the original computes boundXfn from the second click but never uses it; width derives from the clicked height span
ViewportBounds Viewport::completeZoom(double secondDeviceX, double secondDeviceY){
    (void)secondDeviceX;
    Big secondY=planeFromDeviceY(secondDeviceY);
    double approx=std::fabs(Big::sub(pendingPlaneY, secondY).toDouble());
    double current=std::min(spanX(), spanY());
    if(!(approx>0.0) || approx>current){
        approx=current;
    }
    ensurePrecision(approx);
    secondY=planeFromDeviceY(secondDeviceY);
    ViewportBounds target;
    target.yi=secondY;
    target.xi=pendingPlaneX;
    target.xf=Big::add(pendingPlaneX, Big::mul(Big::sub(pendingPlaneY, target.yi), aspectRatio));
    target.yf=pendingPlaneY;
    selectionPending=false;
    return target;
}
// centered rect with the same aspect relation as the original: width derives from height span
ViewportBounds Viewport::planAutoZoom(const Big& centerX, const Big& centerY, double divisor){
    if(!(divisor>0.0)){
        divisor=1.0;
    }
    double current=std::min(spanX(), spanY());
    double approx=current/divisor;
    if(!(approx>0.0)){
        approx=current;
    }
    ensurePrecision(approx);
    Big heightSpan=Big::mul(Big::sub(boundYf, boundYi), 1.0/divisor);
    Big widthSpan=Big::mul(heightSpan, aspectRatio);
    if(!(heightSpan>Big()) || !(widthSpan>Big())){
        return getBounds();
    }
    Big halfWidth=Big::mul(widthSpan, 0.5);
    Big halfHeight=Big::mul(heightSpan, 0.5);
    ViewportBounds target;
    target.xi=Big::sub(centerX, halfWidth);
    target.xf=Big::add(centerX, halfWidth);
    target.yi=Big::sub(centerY, halfHeight);
    target.yf=Big::add(centerY, halfHeight);
    // slide a window that fits back inside the current bounds so every contained zoom can glide instead of crossing through black
    const double widthTarget=widthSpan.toDouble();
    const double heightTarget=heightSpan.toDouble();
    if(widthTarget>0.0 && widthTarget<spanX()){
        if(target.xi<boundXi){
            Big shift=Big::sub(boundXi, target.xi);
            target.xi=Big::add(target.xi, shift);
            target.xf=Big::add(target.xf, shift);
        }
        else if(target.xf>boundXf){
            Big shift=Big::sub(target.xf, boundXf);
            target.xi=Big::sub(target.xi, shift);
            target.xf=Big::sub(target.xf, shift);
        }
    }
    if(heightTarget>0.0 && heightTarget<spanY()){
        if(target.yi<boundYi){
            Big shift=Big::sub(boundYi, target.yi);
            target.yi=Big::add(target.yi, shift);
            target.yf=Big::add(target.yf, shift);
        }
        else if(target.yf>boundYf){
            Big shift=Big::sub(target.yf, boundYf);
            target.yi=Big::sub(target.yi, shift);
            target.yf=Big::sub(target.yf, shift);
        }
    }
    return target;
}
ViewportBounds Viewport::scaledBounds(double factor){
    if(!(factor>0.0)){
        factor=1.0;
    }
    ensurePrecision(std::min(spanX(), spanY())*factor/8.0);
    Big centerX=Big::add(boundXi, Big::mul(Big::sub(boundXf, boundXi), 0.5));
    Big centerY=Big::add(boundYi, Big::mul(Big::sub(boundYf, boundYi), 0.5));
    Big halfWidth=Big::mul(Big::sub(boundXf, boundXi), 0.5*factor);
    Big halfHeight=Big::mul(Big::sub(boundYf, boundYi), 0.5*factor);
    ViewportBounds target;
    target.xi=Big::sub(centerX, halfWidth);
    target.xf=Big::add(centerX, halfWidth);
    target.yi=Big::sub(centerY, halfHeight);
    target.yf=Big::add(centerY, halfHeight);
    return target;
}
ViewportBounds Viewport::initialBounds() const{
    const int limbs=Big::limbsForSpan(0.5);
    ViewportBounds bounds;
    bounds.xi=Big::fromDouble(-2.0, limbs);
    bounds.xf=Big::fromDouble(2.0, limbs);
    bounds.yi=Big::fromDouble(-2.0*static_cast<double>(cssHeight)/static_cast<double>(cssWidth), limbs);
    bounds.yf=Big::fromDouble(2.0*static_cast<double>(cssHeight)/static_cast<double>(cssWidth), limbs);
    return bounds;
}
void Viewport::setBounds(const ViewportBounds& bounds){
    if(!(bounds.xf>bounds.xi) || !(bounds.yf>bounds.yi)){
        return;
    }
    boundXi=bounds.xi;
    boundXf=bounds.xf;
    boundYi=bounds.yi;
    boundYf=bounds.yf;
    ensurePrecision(std::min(spanX(), spanY())/8.0);
    selectionPending=false;
    pendingPlaneX=Big();
    pendingPlaneY=Big();
}
ViewportBounds Viewport::getBounds() const{
    ViewportBounds bounds;
    bounds.xi=boundXi;
    bounds.xf=boundXf;
    bounds.yi=boundYi;
    bounds.yf=boundYf;
    return bounds;
}
Big Viewport::getXi() const{
    return boundXi;
}
Big Viewport::getXf() const{
    return boundXf;
}
Big Viewport::getYi() const{
    return boundYi;
}
Big Viewport::getYf() const{
    return boundYf;
}
Big Viewport::planeFromDeviceX(double deviceX) const{
    return Big::add(boundXi, Big::mul(Big::sub(boundXf, boundXi), deviceX/static_cast<double>(deviceWidth)));
}
Big Viewport::planeFromDeviceY(double deviceY) const{
    return Big::add(boundYf, Big::mul(Big::sub(boundYi, boundYf), deviceY/static_cast<double>(deviceHeight)));
}
Big Viewport::planeAtUnitX(double u) const{
    return Big::add(boundXi, Big::mul(Big::sub(boundXf, boundXi), u));
}
Big Viewport::planeAtUnitY(double v) const{
    return Big::add(boundYi, Big::mul(Big::sub(boundYf, boundYi), v));
}
void Viewport::log(std::ostream& stream) const{
    double sx=spanX();
    double mag=sx>0.0?(4.0/sx):std::numeric_limits<double>::infinity();
    if(std::isfinite(mag) && mag<1e15){
        stream<<"MAG: "<<static_cast<long long>(std::floor(mag))<<"x"<<"\n";
    }
    else{
        stream<<"MAG: "<<std::scientific<<std::setprecision(3)<<mag<<"x"<<"\n";
    }
    stream<<"<"<<boundXi.toString()<<","<<boundYi.toString()<<">"<<"\n";
    stream<<"<"<<boundXf.toString()<<","<<boundYf.toString()<<">"<<std::endl;
}
