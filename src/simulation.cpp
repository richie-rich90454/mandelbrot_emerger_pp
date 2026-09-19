#include "simulation.h"
#include "viewport.h"
#include "color_scheme.h"
#include <cstddef>
#include <thread>
namespace{
    template<class Function>
    void parallelRows(int height, const Function& function){
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        function(0, height);
#else
        unsigned int threadCount=std::thread::hardware_concurrency();
        if(threadCount==0){
            threadCount=1;
        }
        if(threadCount>static_cast<unsigned int>(height)){
            threadCount=static_cast<unsigned int>(height);
        }
        int chunk=height/static_cast<int>(threadCount);
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for(unsigned int t=0; t<threadCount; t++){
            int startRow=static_cast<int>(t)*chunk;
            int endRow=(t==threadCount-1)?height:startRow+chunk;
            workers.push_back(std::thread(function, startRow, endRow));
        }
        for(std::size_t t=0; t<workers.size(); t++){
            workers[t].join();
        }
#endif
    }
}
Simulation::Simulation(int deviceWidth, int deviceHeight):width(deviceWidth),height(deviceHeight),z(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*2u, 0.0),diverge(static_cast<std::size_t>(width)*static_cast<std::size_t>(height), 0.0),planeX(static_cast<std::size_t>(width), 0.0),planeY(static_cast<std::size_t>(height), 0.0),frameIndex(0){
}
void Simulation::reframe(const Viewport& viewport){
    for(int x=0; x<width; x++){
        planeX[static_cast<std::size_t>(x)]=viewport.planeFromDeviceX(static_cast<double>(x));
    }
    for(int y=0; y<height; y++){
        planeY[static_cast<std::size_t>(y)]=viewport.planeFromDeviceY(static_cast<double>(y));
    }
    parallelRows(height, [this](int startRow, int endRow){
        for(int y=startRow; y<endRow; y++){
            const std::size_t row=static_cast<std::size_t>(y);
            double* rowZ=z.data()+row*static_cast<std::size_t>(width)*2u;
            double* rowDiverge=diverge.data()+row*static_cast<std::size_t>(width);
            const double cy=planeY[row];
            for(int x=0; x<width; x++){
                const std::size_t column=static_cast<std::size_t>(x);
                rowZ[column*2u]=planeX[column];
                rowZ[column*2u+1u]=cy;
                rowDiverge[column]=0.0;
            }
        }
    });
}
// one rendered frame's worth of lockstep iterations: each not-yet-escaped point advances passCount times,
// escapes are stamped with the exact pass that fails the magnitude test, and colors are computed once
// against the frame index after the batch - the same bytes the per-pass loop produced at that index
void Simulation::step(int passCount, const ColorScheme* scheme, unsigned char* pixels, int pitch){
    const long long baseIndex=frameIndex;
    frameIndex+=passCount;
    const long long divisor=frameIndex;
    parallelRows(height, [this, passCount, baseIndex, divisor, scheme, pixels, pitch](int startRow, int endRow){
        renderRows(startRow, endRow, passCount, baseIndex, divisor, scheme, pixels, pitch);
    });
}
void Simulation::renderRows(int startRow, int endRow, int passCount, long long baseIndex, long long divisor, const ColorScheme* scheme, unsigned char* pixels, int pitch){
    for(int y=startRow; y<endRow; y++){
        const std::size_t row=static_cast<std::size_t>(y);
        double* rowZ=z.data()+row*static_cast<std::size_t>(width)*2u;
        double* rowDiverge=diverge.data()+row*static_cast<std::size_t>(width);
        unsigned char* rowPixels=pixels+row*static_cast<std::size_t>(pitch);
        const double cy=planeY[row];
        for(int x=0; x<width; x++){
            const std::size_t column=static_cast<std::size_t>(x);
            double escapeFrame=rowDiverge[column];
            if(escapeFrame==0.0){
                double re=rowZ[column*2u];
                double im=rowZ[column*2u+1u];
                const double cx=planeX[column];
                for(int pass=1; pass<=passCount; pass++){
                    if(re*re+im*im<=4.0){
                        const double nextRe=re*re-im*im+cx;
                        im=2.0*re*im+cy;
                        re=nextRe;
                    }
                    else{
                        escapeFrame=static_cast<double>(baseIndex+pass);
                        rowDiverge[column]=escapeFrame;
                        break;
                    }
                }
                if(escapeFrame==0.0){
                    rowZ[column*2u]=re;
                    rowZ[column*2u+1u]=im;
                }
            }
            double brightness=0.0;
            if(escapeFrame!=0.0){
                brightness=escapeFrame/static_cast<double>(divisor)*255.0;
            }
            Rgba color;
            scheme->shade(brightness, color);
            unsigned char* pixel=rowPixels+column*4u;
            pixel[0]=color.r;
            pixel[1]=color.g;
            pixel[2]=color.b;
            pixel[3]=color.a;
        }
    }
}
