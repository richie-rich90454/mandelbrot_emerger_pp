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
    virtual const char* name() const=0;
protected:
    static unsigned char clampChannel(double value);
};
class GrayscaleScheme:public ColorScheme{
public:
    GrayscaleScheme();
    virtual ~GrayscaleScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
};
class ThermalScheme:public ColorScheme{
public:
    ThermalScheme();
    virtual ~ThermalScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
};
class AlphaScheme:public ColorScheme{
public:
    AlphaScheme();
    virtual ~AlphaScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
};
class RainbowScheme:public ColorScheme{
public:
    RainbowScheme();
    virtual ~RainbowScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
};
class FireScheme:public ColorScheme{
public:
    FireScheme();
    virtual ~FireScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
};
class IceScheme:public ColorScheme{
public:
    IceScheme();
    virtual ~IceScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
};
class AmberScheme:public ColorScheme{
public:
    AmberScheme();
    virtual ~AmberScheme();
    virtual void shade(double brightness, Rgba& out) const override;
    virtual const char* name() const override;
private:
    Rgba ramp[256];
};
#endif
