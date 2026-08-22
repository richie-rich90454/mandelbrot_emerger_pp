#ifndef COLOR_SCHEME_H
#define COLOR_SCHEME_H
struct Rgba{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};
class ColorScheme{
public:
    ColorScheme();
    virtual ~ColorScheme();
    virtual void shade(double brightness, Rgba& out) const=0;
protected:
    static unsigned char clampChannel(double value);
};
class GrayscaleScheme:public ColorScheme{
public:
    GrayscaleScheme();
    virtual ~GrayscaleScheme();
    virtual void shade(double brightness, Rgba& out) const override;
};
class ThermalScheme:public ColorScheme{
public:
    ThermalScheme();
    virtual ~ThermalScheme();
    virtual void shade(double brightness, Rgba& out) const override;
};
class AlphaScheme:public ColorScheme{
public:
    AlphaScheme();
    virtual ~AlphaScheme();
    virtual void shade(double brightness, Rgba& out) const override;
};
#endif
