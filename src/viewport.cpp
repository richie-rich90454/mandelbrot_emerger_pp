#include "viewport.h"
#include <cmath>
#include <ostream>
Viewport::Viewport(int cssWidth, int cssHeight):cssWidth(cssWidth),cssHeight(cssHeight),aspectRatio(static_cast<double>(cssWidth)/static_cast<double>(cssHeight)),boundXi(0.0),boundXf(0.0),boundYi(0.0),boundYf(0.0),selectionPending(false),pendingPlaneX(0.0),pendingPlaneY(0.0){
    initializeBounds();
}
void Viewport::initializeBounds(){
    boundXi=-2.0;
    boundXf=2.0;
    boundYi=-2.0*static_cast<double>(cssHeight)/static_cast<double>(cssWidth);
    boundYf=2.0*static_cast<double>(cssHeight)/static_cast<double>(cssWidth);
    selectionPending=false;
    pendingPlaneX=0.0;
    pendingPlaneY=0.0;
}
void Viewport::beginZoom(double deviceX, double deviceY){
    pendingPlaneX=planeFromDeviceX(deviceX);
    pendingPlaneY=planeFromDeviceY(deviceY);
    selectionPending=true;
}
// the original computes boundXfn from the second click but never uses it; width derives from the clicked height span
void Viewport::completeZoom(double secondDeviceX, double secondDeviceY){
    (void)secondDeviceX;
    double newYi=planeFromDeviceY(secondDeviceY);
    double newXf=pendingPlaneX+(pendingPlaneY-newYi)*aspectRatio;
    boundXi=pendingPlaneX;
    boundXf=newXf;
    boundYi=newYi;
    boundYf=pendingPlaneY;
    selectionPending=false;
}
// centered rect with the same aspect relation as the original: width derives from height span
void Viewport::autoZoom(double centerX, double centerY, double divisor){
    double heightSpan=(boundYf-boundYi)/divisor;
    double widthSpan=heightSpan*aspectRatio;
    if(!(widthSpan>0.0) || !std::isfinite(widthSpan)){
        return;
    }
    boundXi=centerX-widthSpan*0.5;
    boundXf=centerX+widthSpan*0.5;
    boundYi=centerY-heightSpan*0.5;
    boundYf=centerY+heightSpan*0.5;
    selectionPending=false;
    pendingPlaneX=0.0;
    pendingPlaneY=0.0;
}
void Viewport::resetToInitial(){
    initializeBounds();
}
double Viewport::getXi() const{
    return boundXi;
}
double Viewport::getXf() const{
    return boundXf;
}
double Viewport::getYi() const{
    return boundYi;
}
double Viewport::getYf() const{
    return boundYf;
}
double Viewport::planeFromDeviceX(double deviceX) const{
    double span=static_cast<double>(cssWidth*RES);
    return boundXi+(deviceX/span)*(boundXf-boundXi);
}
double Viewport::planeFromDeviceY(double deviceY) const{
    double span=static_cast<double>(cssHeight*RES);
    return boundYf+(deviceY/span)*(boundYi-boundYf);
}
void Viewport::log(std::ostream& stream) const{
    stream<<"MAG: "<<static_cast<long long>(std::floor(4.0/(boundXf-boundXi)))<<"x"<<"\n";
    stream<<"<"<<boundXi<<","<<boundYi<<">"<<"\n";
    stream<<"<"<<boundXf<<","<<boundYf<<">"<<std::endl;
}
