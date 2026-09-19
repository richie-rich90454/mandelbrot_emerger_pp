#include "simulation.h"
#include "viewport.h"
#include "color_scheme.h"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
namespace{
    // the reference orbit has to outlive the epoch by enough passes that rounding cannot kill it early
    const long long PROBE_REFERENCE_MARGIN=16;
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
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
#endif
    template<class Function>
    void parallelRows(int height, const Function& function){
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        function(0, height);
#else
        rowPool().run(height, function);
#endif
    }
}
Simulation::Simulation(int deviceWidth, int deviceHeight):width(deviceWidth),height(deviceHeight),w(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*2u, 0.0),refIndex(static_cast<std::size_t>(width)*static_cast<std::size_t>(height), 1u),diverge(static_cast<std::size_t>(width)*static_cast<std::size_t>(height), 0.0),dcX(static_cast<std::size_t>(width), 0.0),dcY(static_cast<std::size_t>(height), 0.0),toneTable(1, 0.0),spanX(1.0),spanY(1.0),refU(0.5),refV(0.5),cRefRe(),cRefIm(),orbitRe(),orbitIm(),refZr(),refZi(),refD(),frameIndex(0),reframeIndex(0),refEscaped(false){
}
// the reference orbit is the view center (or a probe-chosen off-center point) in arbitrary precision binary
// fixed point: index 0 is the critical point start, index n is z_n for that c, and every pixel rides it as
// z_pixel = Z_index + w with w kept in doubles, so one high-precision orbit covers the whole frame
void Simulation::reframe(const Viewport& viewport, double referenceU, double referenceV){
    reframeIndex=frameIndex;
    refU=referenceU;
    refV=referenceV;
    spanX=viewport.spanX();
    spanY=viewport.spanY();
    Big xi=viewport.getXi();
    Big xf=viewport.getXf();
    Big yi=viewport.getYi();
    Big yf=viewport.getYf();
    Big widthSpan=Big::sub(xf, xi);
    Big heightSpan=Big::sub(yf, yi);
    cRefRe=Big::add(xi, Big::mul(widthSpan, refU));
    cRefIm=Big::add(yi, Big::mul(heightSpan, refV));
    for(int x=0; x<width; x++){
        dcX[static_cast<std::size_t>(x)]=(static_cast<double>(x)/static_cast<double>(width)-refU)*spanX;
    }
    for(int y=0; y<height; y++){
        const std::size_t row=static_cast<std::size_t>(y);
        dcY[row]=(1.0-static_cast<double>(y)/static_cast<double>(height)-refV)*spanY;
        double* rowW=w.data()+row*static_cast<std::size_t>(width)*2u;
        for(int x=0; x<width; x++){
            rowW[static_cast<std::size_t>(x)*2u]=dcX[static_cast<std::size_t>(x)];
            rowW[static_cast<std::size_t>(x)*2u+1u]=dcY[row];
        }
    }
    std::fill(refIndex.begin(), refIndex.end(), 1u);
    std::fill(diverge.begin(), diverge.end(), 0.0);
    refZr.clear();
    refZi.clear();
    refD.clear();
    refZr.push_back(0.0);
    refZi.push_back(0.0);
    refD.push_back(-4.0);
    orbitRe=cRefRe;
    orbitIm=cRefIm;
    refEscaped=false;
    appendReferenceEntry();
}
void Simulation::appendReferenceEntry(){
    refZr.push_back(orbitRe.toDouble());
    refZi.push_back(orbitIm.toDouble());
    Big magnitude=Big::add(Big::mul(orbitRe, orbitRe), Big::mul(orbitIm, orbitIm));
    double d=Big::sub(magnitude, Big::fromDouble(4.0, static_cast<int>(magnitude.limbs()))).toDouble();
    refD.push_back(d);
    if(d>0.0){
        refEscaped=true;
    }
}
void Simulation::extendReference(long long count){
    while(static_cast<long long>(refZr.size())<count && !refEscaped){
        Big nextRe=Big::add(Big::sub(Big::mul(orbitRe, orbitRe), Big::mul(orbitIm, orbitIm)), cRefRe);
        Big nextIm=Big::add(Big::mul(Big::mul(orbitRe, orbitIm), 2.0), cRefIm);
        orbitRe=nextRe;
        orbitIm=nextIm;
        appendReferenceEntry();
    }
}
// one rendered frame's worth of lockstep iterations: each not-yet-escaped point advances passCount times,
// escapes are stamped with the pass that fails the magnitude test, and colors are computed once per frame
// from a lookup table of the escape count, so the per-pixel cost is a load and a multiply
void Simulation::step(int passCount, const ColorScheme* scheme, unsigned char* pixels, int pitch){
    const long long baseIndex=frameIndex;
    frameIndex+=passCount;
    const long long span=frameIndex-reframeIndex;
    extendReference(span+1);
    while(static_cast<long long>(toneTable.size())<=span){
        toneTable.push_back(255.0*std::pow(static_cast<double>(toneTable.size()), 0.75));
    }
    const double toneScale=std::pow(static_cast<double>(span), -0.75);
    parallelRows(height, [this, passCount, baseIndex, toneScale, scheme, pixels, pitch](int startRow, int endRow){
        renderRows(startRow, endRow, passCount, baseIndex, toneScale, scheme, pixels, pitch);
    });
}
long long Simulation::passesSinceReframe() const{
    return frameIndex-reframeIndex;
}
double Simulation::escapeCountAt(int x, int y) const{
    return diverge[static_cast<std::size_t>(y)*static_cast<std::size_t>(width)+static_cast<std::size_t>(x)];
}
bool Simulation::referenceEscaped() const{
    return refEscaped;
}
bool Simulation::bestReferenceOffset(double& u, double& v) const{
    double best=0.0;
    bool found=false;
    int bestX=width/2;
    int bestY=height/2;
    for(int y=0; y<height; y++){
        const std::size_t row=static_cast<std::size_t>(y);
        for(int x=0; x<width; x++){
            const std::size_t index=row*static_cast<std::size_t>(width)+static_cast<std::size_t>(x);
            if(diverge[index]!=0.0){
                continue;
            }
            const unsigned int k=refIndex[index];
            if(static_cast<std::size_t>(k)>=refZr.size()){
                continue;
            }
            const double zr=refZr[k]+w[index*2u];
            const double zi=refZi[k]+w[index*2u+1u];
            const double magnitude=zr*zr+zi*zi;
            if(!found || magnitude<best){
                found=true;
                best=magnitude;
                bestX=x;
                bestY=y;
            }
        }
    }
    u=(static_cast<double>(bestX)+0.5)/static_cast<double>(width);
    v=1.0-(static_cast<double>(bestY)+0.5)/static_cast<double>(height);
    return found;
}
// steers toward structure the same way the direct renderer did, but reads survival off the shared high-precision
// reference orbit, so it keeps separating candidates at depths where every double coordinate would be identical:
// the slowest escaper under the caller's budget is the zoom target (it resolves within the scheduled epoch), the
// slowest escaper under the full horizon is the reference candidate (it outlives that epoch almost to its end),
// and a full-horizon survivor beats both because nothing can outlive it
ProbeResult Simulation::probeStructure(int samples, int horizon, long long targetBudget, long long minTargetIterations, std::mt19937& randomEngine){
    ProbeResult result;
    if(horizon<2){
        horizon=2;
    }
    if(targetBudget<minTargetIterations){
        targetBudget=minTargetIterations;
    }
    extendReference(static_cast<long long>(horizon)+1);
    const long long cacheSize=static_cast<long long>(refZr.size());
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    struct Candidate{
        double u;
        double v;
        long long iterations;
    };
    std::vector<Candidate> escapeCandidates;
    escapeCandidates.reserve(static_cast<std::size_t>(samples));
    double bestSurvivorMagnitude=0.0;
    for(int sample=0; sample<samples; sample++){
        const double u=unit(randomEngine);
        const double v=unit(randomEngine);
        const double dcx=(u-refU)*spanX;
        const double dcy=(v-refV)*spanY;
        double wr=dcx;
        double wi=dcy;
        long long k=1;
        bool escaped=false;
        double magnitude=0.0;
        int iteration=0;
        for(; iteration<horizon; iteration++){
            if(k>=cacheSize){
                escaped=true;
                break;
            }
            const double Zr=refZr[static_cast<std::size_t>(k)];
            const double Zi=refZi[static_cast<std::size_t>(k)];
            const double zr=Zr+wr;
            const double zi=Zi+wi;
            const double wr2=wr*wr;
            const double wi2=wi*wi;
            magnitude=zr*zr+zi*zi;
            if(refD[static_cast<std::size_t>(k)]+2.0*(Zr*wr+Zi*wi)+(wr2+wi2)>0.0){
                escaped=true;
                break;
            }
            if(magnitude<wr2+wi2){
                wr=zr;
                wi=zi;
                k=0;
            }
            const double ur=refZr[static_cast<std::size_t>(k)];
            const double ui=refZi[static_cast<std::size_t>(k)];
            const double nr=2.0*(ur*wr-ui*wi)+(wr*wr-wi*wi)+dcx;
            const double ni=2.0*(ur*wi+ui*wr)+2.0*wr*wi+dcy;
            wr=nr;
            wi=ni;
            k++;
        }
        if(escaped){
            escapeCandidates.push_back(Candidate{u, v, iteration});
        }
        else if(!result.hasSurvivor || magnitude<bestSurvivorMagnitude){
            result.hasSurvivor=true;
            bestSurvivorMagnitude=magnitude;
            result.survivorU=u;
            result.survivorV=v;
        }
    }
    for(std::size_t i=0; i<escapeCandidates.size(); i++){
        if(escapeCandidates[i].iterations<minTargetIterations){
            continue;
        }
        if(!result.hasSlowest || escapeCandidates[i].iterations>result.slowestIterations){
            result.hasSlowest=true;
            result.slowestU=escapeCandidates[i].u;
            result.slowestV=escapeCandidates[i].v;
            result.slowestIterations=escapeCandidates[i].iterations;
        }
    }
    // the camera target has to resolve before the reference candidate dies, so the budget stops one margin short
    // of the slowest escaper; if nothing resolves that fast, the caller falls back to the slowest point itself
    long long effectiveBudget=targetBudget;
    if(result.hasSlowest && result.slowestIterations-PROBE_REFERENCE_MARGIN<effectiveBudget){
        effectiveBudget=result.slowestIterations-PROBE_REFERENCE_MARGIN;
    }
    for(std::size_t i=0; i<escapeCandidates.size(); i++){
        if(escapeCandidates[i].iterations<minTargetIterations || escapeCandidates[i].iterations>effectiveBudget){
            continue;
        }
        if(!result.hasStructure || escapeCandidates[i].iterations>result.targetIterations){
            result.hasStructure=true;
            result.targetU=escapeCandidates[i].u;
            result.targetV=escapeCandidates[i].v;
            result.targetIterations=escapeCandidates[i].iterations;
        }
    }
    return result;
}
void Simulation::renderRows(int startRow, int endRow, int passCount, long long baseIndex, double toneScale, const ColorScheme* scheme, unsigned char* pixels, int pitch){
    for(int y=startRow; y<endRow; y++){
        const std::size_t row=static_cast<std::size_t>(y);
        double* rowW=w.data()+row*static_cast<std::size_t>(width)*2u;
        unsigned int* rowRef=refIndex.data()+row*static_cast<std::size_t>(width);
        double* rowDiverge=diverge.data()+row*static_cast<std::size_t>(width);
        unsigned char* rowPixels=pixels+row*static_cast<std::size_t>(pitch);
        const double dcy=dcY[row];
        for(int x=0; x<width; x++){
            const std::size_t column=static_cast<std::size_t>(x);
            double escapeCount=rowDiverge[column];
            if(escapeCount==0.0){
                double wr=rowW[column*2u];
                double wi=rowW[column*2u+1u];
                unsigned int k=rowRef[column];
                const double dcx=dcX[column];
                for(int pass=1; pass<=passCount; pass++){
                    if(static_cast<std::size_t>(k)>=refZr.size()){
                        escapeCount=static_cast<double>(baseIndex+pass-reframeIndex);
                        rowDiverge[column]=escapeCount;
                        break;
                    }
                    const double Zr=refZr[k];
                    const double Zi=refZi[k];
                    const double zr=Zr+wr;
                    const double zi=Zi+wi;
                    const double wr2=wr*wr;
                    const double wi2=wi*wi;
                    // magnitude test on |Z+w|^2 - 4 without ever forming the cancelling sum: the reference part is
                    // exact from the precision header, the perturbation part only needs its own scale
                    if(refD[k]+2.0*(Zr*wr+Zi*wi)+(wr2+wi2)>0.0){
                        escapeCount=static_cast<double>(baseIndex+pass-reframeIndex);
                        rowDiverge[column]=escapeCount;
                        break;
                    }
                    // Zhuoran rebasing: when the pixel orbit passes near the critical point the delta is
                    // re-anchored to the orbit start and keeps its double precision, so deep orbits never glitch
                    if(zr*zr+zi*zi<wr2+wi2){
                        wr=zr;
                        wi=zi;
                        k=0;
                    }
                    const double ur=refZr[k];
                    const double ui=refZi[k];
                    const double nextRe=2.0*(ur*wr-ui*wi)+(wr*wr-wi*wi)+dcx;
                    const double nextIm=2.0*(ur*wi+ui*wr)+2.0*wr*wi+dcy;
                    wr=nextRe;
                    wi=nextIm;
                    k++;
                }
                if(escapeCount==0.0){
                    rowW[column*2u]=wr;
                    rowW[column*2u+1u]=wi;
                    rowRef[column]=k;
                }
            }
            double brightness=0.0;
            if(escapeCount!=0.0){
                // table holds count^0.75; scaling by span^-0.75 gives a monotone fade, so the slowest escapers are
                // always the brightest and every filament reads as a bright ridge against the dimmer field
                brightness=toneTable[static_cast<std::size_t>(escapeCount)]*toneScale;
            }
            Rgba color;
            scheme->shade(brightness, color);
            unsigned char* pixel=rowPixels+column*4u;
            pixel[0]=color.r;
            pixel[1]=color.g;
            pixel[2]=color.b;
            pixel[3]=255;
        }
    }
}
