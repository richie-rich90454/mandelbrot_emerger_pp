#include "simulation.h"
#include "viewport.h"
#include "color_scheme.h"
#include <cmath>
#include <thread>
Simulation::Simulation(int deviceWidth, int deviceHeight):width(deviceWidth),height(deviceHeight),buffer(width,height),container(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*2u, 0.0),coords(container.size(), 0.0),diverge(container.size(), 0.0){
}
void Simulation::reframe(const Viewport& viewport){
    for(int y=0; y<height; y++){
        for(int x=0; x<width; x++){
            int i=(y*width+x)*2;
            double re=viewport.planeFromDeviceX(static_cast<double>(x));
            double im=viewport.planeFromDeviceY(static_cast<double>(y));
            container[i]=re;
            container[i+1]=im;
            coords[i]=re;
            coords[i+1]=im;
            diverge[i]=0.0;
            diverge[i+1]=0.0;
        }
    }
}
// brightness of escaped points fades as the global frame counter grows and never resets on zoom, matching sketch.js
void Simulation::step(long long frameIndex, const ColorScheme* scheme){
    unsigned int threadCount=std::thread::hardware_concurrency();
    if(threadCount==0){
        threadCount=1;
    }
    if(threadCount>static_cast<unsigned int>(height)){
        threadCount=static_cast<unsigned int>(height);
    }
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    int chunk=height/static_cast<int>(threadCount);
    for(unsigned int t=0; t<threadCount; t++){
        int startRow=static_cast<int>(t)*chunk;
        int endRow=(t==threadCount-1)?height:startRow+chunk;
        workers.push_back(std::thread(&Simulation::renderRows, this, startRow, endRow, frameIndex, scheme));
    }
    for(std::size_t t=0; t<workers.size(); t++){
        workers[t].join();
    }
}
void Simulation::renderRows(int startRow, int endRow, long long frameIndex, const ColorScheme* scheme){
    unsigned char* pixels=buffer.data();
    for(int y=startRow; y<endRow; y++){
        for(int x=0; x<width; x++){
            int i=(y*width+x)*2;
            double xi=container[i];
            double yi=container[i+1];
            double brightness=0.0;
            if(diverge[i+1]==0.0 && std::sqrt(xi*xi+yi*yi)<=2.0){
                container[i]=xi*xi-yi*yi+coords[i];
                container[i+1]=2.0*xi*yi+coords[i+1];
            }
            else{
                if(diverge[i+1]==0.0){
                    diverge[i+1]=1.0;
                    diverge[i]=static_cast<double>(frameIndex);
                }
                brightness=diverge[i]/static_cast<double>(frameIndex)*255.0;
            }
            Rgba color;
            scheme->shade(brightness, color);
            unsigned char* pixel=pixels+(static_cast<std::size_t>(y*width+x)*4u);
            pixel[0]=color.r;
            pixel[1]=color.g;
            pixel[2]=color.b;
            pixel[3]=color.a;
        }
    }
}
const PixelBuffer& Simulation::getBuffer() const{
    return buffer;
}
