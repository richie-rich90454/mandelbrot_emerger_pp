#ifndef VIEWPORT_H
#define VIEWPORT_H
#include <iosfwd>
const int RES=2;
struct ViewportBounds{
    double xi;
    double xf;
    double yi;
    double yf;
};
class Viewport{
public:
    Viewport(int cssWidth, int cssHeight);
    void beginZoom(double deviceX, double deviceY);
    ViewportBounds completeZoom(double secondDeviceX, double secondDeviceY);
    ViewportBounds planAutoZoom(double centerX, double centerY, double divisor) const;
    void setBounds(const ViewportBounds& bounds);
    ViewportBounds getBounds() const;
    void resetToInitial();
    double getXi() const;
    double getXf() const;
    double getYi() const;
    double getYf() const;
    double planeFromDeviceX(double deviceX) const;
    double planeFromDeviceY(double deviceY) const;
    void log(std::ostream& stream) const;
private:
    void initializeBounds();
    int cssWidth;
    int cssHeight;
    double aspectRatio;
    double boundXi;
    double boundXf;
    double boundYi;
    double boundYf;
    bool selectionPending;
    double pendingPlaneX;
    double pendingPlaneY;
};
#endif
