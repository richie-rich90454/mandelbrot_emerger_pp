#ifndef BIG_H
#define BIG_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
// arbitrary-precision fixed point: value = sign * sum(m[i] * 2^(-64 i)), limb 0 holds the integer part.
// limb count grows with zoom depth, so coordinates keep resolving pixels far past the double wall.
class Big{
public:
    Big():m(1, 0),negative(false){
    }
    bool isZero() const{
        for(std::size_t i=0; i<m.size(); i++){
            if(m[i]!=0){
                return false;
            }
        }
        return true;
    }
    std::size_t limbs() const{
        return m.size();
    }
    Big withLimbs(int n) const{
        if(n<=0 || static_cast<std::size_t>(n)<=m.size()){
            return *this;
        }
        Big r=*this;
        r.m.resize(static_cast<std::size_t>(n), 0);
        return r;
    }
    // limbs needed so a view of this span keeps 96 bits of working margin below it; limb 0 is the integer part,
    // so every limb above it buys 64 fraction bits
    static int limbsForSpan(double span){
        if(!(span>0.0) || !std::isfinite(span)){
            return 3;
        }
        double fractionBits=96.0-std::log2(span);
        if(fractionBits<64.0){
            fractionBits=64.0;
        }
        return 1+static_cast<int>(std::ceil(fractionBits/64.0));
    }

    static Big fromDouble(double x, int minLimbs=1){
        if(minLimbs<1){
            minLimbs=1;
        }
        Big r;
        r.m.clear();
        r.negative=std::signbit(x) && x!=0.0;
        double t=std::fabs(x);
        if(!std::isfinite(t)){
            t=0.0;
        }
        int produced=0;
        while(produced<minLimbs || t>0.0){
            double ip=std::floor(t);
            uint64_t limb=0;
            if(ip>=18446744073709551615.0){
                limb=~static_cast<uint64_t>(0);
            }
            else{
                limb=static_cast<uint64_t>(ip);
            }
            r.m.push_back(limb);
            t=(t-ip)*18446744073709551616.0;
            produced++;
            if(produced>=4096){
                break;
            }
        }
        if(r.m.empty()){
            r.m.push_back(0);
        }
        r.normalizeSign();
        return r;
    }
    double toDouble() const{
        double t=0.0;
        for(std::size_t i=m.size(); i-- > 0; ){
            t=t*0x1p-64+static_cast<double>(m[i]);
        }
        return negative?-t:t;
    }
    std::string toString(int fracDigits=30) const{
        std::string s;
        if(negative && !isZero()){
            s+='-';
        }
        uint64_t ip=m.empty()?0:m[0];
        if(ip==0){
            s+='0';
        }
        else{
            char buffer[32];
            int count=0;
            while(ip>0){
                buffer[count++]=static_cast<char>('0'+static_cast<int>(ip%10));
                ip/=10;
            }
            while(count>0){
                s+=buffer[--count];
            }
        }
        if(fracDigits>0 && m.size()>1){
            std::vector<uint64_t> frac(m.begin()+1, m.end());
            std::string digits;
            for(int digit=0; digit<fracDigits; digit++){
                uint64_t carry=0;
                for(std::size_t i=frac.size(); i-- > 0; ){
                    uint64_t lo=0;
                    uint64_t hi=0;
                    mul64(frac[i], 10, lo, hi);
                    uint64_t sum=lo+carry;
                    uint64_t carryOut=(sum<lo)?1u:0u;
                    frac[i]=sum;
                    carry=hi+carryOut;
                }
                digits+=static_cast<char>('0'+static_cast<int>(carry%10));
            }
            std::size_t end=digits.size();
            while(end>0 && digits[end-1]=='0'){
                end--;
            }
            if(end>0){
                s+='.';
                s.append(digits, 0, end);
            }
        }
        return s;
    }
    static int cmp(const Big& a, const Big& b){
        bool az=a.isZero();
        bool bz=b.isZero();
        if(az && bz){
            return 0;
        }
        if(a.negative!=b.negative){
            return a.negative?-1:1;
        }
        int mag=cmpMag(a.m, b.m);
        return a.negative?-mag:mag;
    }
    static Big add(const Big& a, const Big& b){
        if(a.negative==b.negative){
            Big r;
            r.m=addMag(a.m, b.m);
            r.negative=a.negative;
            r.normalizeSign();
            return r;
        }
        int mag=cmpMag(a.m, b.m);
        if(mag==0){
            return Big();
        }
        Big r;
        if(mag>0){
            r.m=subMag(a.m, b.m);
            r.negative=a.negative;
        }
        else{
            r.m=subMag(b.m, a.m);
            r.negative=b.negative;
        }
        r.normalizeSign();
        return r;
    }
    static Big sub(const Big& a, const Big& b){
        return add(a, -b);
    }
    static Big mul(const Big& a, const Big& b){
        const std::size_t n=std::max(a.m.size(), b.m.size());
        std::vector<uint64_t> r(n, 0);
        for(std::size_t i=0; i<a.m.size(); i++){
            for(std::size_t j=0; j<b.m.size(); j++){
                uint64_t lo=0;
                uint64_t hi=0;
                mul64(a.m[i], b.m[j], lo, hi);
                // weights descend with the index, so a 128-bit limb product splits upward into index i+j-1
                addLimb(r, static_cast<long long>(i+j), lo);
                addLimb(r, static_cast<long long>(i+j)-1, hi);
            }
        }
        Big out;
        out.m=std::move(r);
        out.negative=a.negative!=b.negative;
        out.normalizeSign();
        return out;
    }
    static Big mul(const Big& a, double b){
        return mul(a, fromDouble(b, static_cast<int>(a.limbs())));
    }
    static Big add(const Big& a, double b){
        return add(a, fromDouble(b, static_cast<int>(a.limbs())));
    }
    Big operator-() const{
        Big r=*this;
        if(!r.isZero()){
            r.negative=!r.negative;
        }
        return r;
    }
    friend Big operator+(const Big& a, const Big& b){
        return add(a, b);
    }
    friend Big operator-(const Big& a, const Big& b){
        return sub(a, b);
    }
    friend Big operator*(const Big& a, const Big& b){
        return mul(a, b);
    }
    friend bool operator==(const Big& a, const Big& b){
        return cmp(a, b)==0;
    }
    friend bool operator!=(const Big& a, const Big& b){
        return cmp(a, b)!=0;
    }
    friend bool operator<(const Big& a, const Big& b){
        return cmp(a, b)<0;
    }
    friend bool operator<=(const Big& a, const Big& b){
        return cmp(a, b)<=0;
    }
    friend bool operator>(const Big& a, const Big& b){
        return cmp(a, b)>0;
    }
    friend bool operator>=(const Big& a, const Big& b){
        return cmp(a, b)>=0;
    }
private:
    static void mul64(uint64_t a, uint64_t b, uint64_t& lo, uint64_t& hi){
        uint64_t a0=a&0xffffffffu;
        uint64_t a1=a>>32;
        uint64_t b0=b&0xffffffffu;
        uint64_t b1=b>>32;
        uint64_t p00=a0*b0;
        uint64_t p01=a0*b1;
        uint64_t p10=a1*b0;
        uint64_t p11=a1*b1;
        uint64_t mid=(p00>>32)+(p01&0xffffffffu)+(p10&0xffffffffu);
        lo=(mid<<32)|(p00&0xffffffffu);
        hi=p11+(p01>>32)+(p10>>32)+(mid>>32);
    }
    static void addLimb(std::vector<uint64_t>& r, long long index, uint64_t value){
        while(value!=0 && index>=0){
            if(index>=static_cast<long long>(r.size())){
                return;
            }
            uint64_t sum=r[static_cast<std::size_t>(index)]+value;
            uint64_t carry=(sum<value)?1u:0u;
            r[static_cast<std::size_t>(index)]=sum;
            value=carry;
            index--;
        }
    }
    static int cmpMag(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b){
        const std::size_t n=std::max(a.size(), b.size());
        for(std::size_t i=0; i<n; i++){
            uint64_t av=(i<a.size())?a[i]:0;
            uint64_t bv=(i<b.size())?b[i]:0;
            if(av!=bv){
                return av<bv?-1:1;
            }
        }
        return 0;
    }
    static std::vector<uint64_t> addMag(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b){
        const std::size_t n=std::max(a.size(), b.size());
        std::vector<uint64_t> r(n, 0);
        uint64_t carry=0;
        for(std::size_t k=n; k-- > 0; ){
            uint64_t av=(k<a.size())?a[k]:0;
            uint64_t bv=(k<b.size())?b[k]:0;
            uint64_t sum=av+bv;
            uint64_t carryLow=(sum<av)?1u:0u;
            uint64_t sum2=sum+carry;
            uint64_t carryMid=(sum2<sum)?1u:0u;
            r[k]=sum2;
            carry=carryLow|carryMid;
        }
        // a carry out of limb 0 means the integer part exceeded 64 bits; engine values stay far below that
        return r;
    }
    static std::vector<uint64_t> subMag(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b){
        const std::size_t n=std::max(a.size(), b.size());
        std::vector<uint64_t> r(n, 0);
        uint64_t borrow=0;
        for(std::size_t k=n; k-- > 0; ){
            uint64_t av=(k<a.size())?a[k]:0;
            uint64_t bv=(k<b.size())?b[k]:0;
            uint64_t diff=av-bv;
            uint64_t borrowLow=(av<bv)?1u:0u;
            uint64_t diff2=diff-borrow;
            uint64_t borrowMid=(diff<borrow)?1u:0u;
            r[k]=diff2;
            borrow=borrowLow|borrowMid;
        }
        return r;
    }
    void normalizeSign(){
        if(isZero()){
            negative=false;
        }
    }
    std::vector<uint64_t> m;
    bool negative;
};
#endif
