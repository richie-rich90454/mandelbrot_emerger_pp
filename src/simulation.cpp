#include "simulation.h"
#include "viewport.h"
#include "color_scheme.h"
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
namespace{
    // spawning a worker per pass cost the desktop most of its frame budget; the threads now live for the process lifetime
    class RowPool{
    public:
        RowPool(){
            unsigned int count=std::thread::hardware_concurrency();
            if(count==0){
                count=1;
            }
            workers.reserve(count);
            for(unsigned int i=0; i<count; i++){
                workers.emplace_back([this, i]{ loop(i); });
            }
        }
        ~RowPool(){
            {
                std::lock_guard<std::mutex> lock(mutex);
                stopping=true;
            }
            wake.notify_all();
            for(std::size_t i=0; i<workers.size(); i++){
                workers[i].join();
            }
        }
        void run(int height, const std::function<void(int, int)>& task){
            if(workers.empty() || height<=0){
                task(0, height);
                return;
            }
            std::unique_lock<std::mutex> lock(mutex);
            current=task;
            rows=height;
            remaining=static_cast<int>(workers.size());
            generation++;
            wake.notify_all();
            finished.wait(lock, [this]{ return remaining==0; });
            current=nullptr;
        }
    private:
        void loop(unsigned int index){
            std::unique_lock<std::mutex> lock(mutex);
            unsigned long long seen=0;
            while(true){
                wake.wait(lock, [this, seen]{ return stopping || generation!=seen; });
                if(stopping){
                    return;
                }
                seen=generation;
                const int workerCount=static_cast<int>(workers.size());
                const int chunk=rows/workerCount;
                const int start=static_cast<int>(index)*chunk;
                const int end=(index==static_cast<unsigned int>(workerCount)-1)?rows:start+chunk;
                const std::function<void(int, int)> task=current;
                lock.unlock();
                if(start<end){
                    task(start, end);
                }
                lock.lock();
                if(--remaining==0){
                    finished.notify_one();
                }
            }
        }
        std::vector<std::thread> workers;
        std::mutex mutex;
        std::condition_variable wake;
        std::condition_variable finished;
        std::function<void(int, int)> current;
        int rows=0;
        int remaining=0;
        unsigned long long generation=0;
        bool stopping=false;
    };
    RowPool& rowPool(){
        static RowPool pool;
        return pool;
    }
    template<class Function>
    void parallelRows(int height, const Function& function){
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        function(0, height);
#else
        rowPool().run(height, function);
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
