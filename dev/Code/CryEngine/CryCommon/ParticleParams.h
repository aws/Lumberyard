/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* diretchribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYCOMMON_PARTICLEPARAMS_H
#define CRYINCLUDE_CRYCOMMON_PARTICLEPARAMS_H
#pragma once


#include <FinalizingSpline.h>
#include <Cry_Color.h>
#include <CryString.h>
#include <CryName.h>

#include <CryCustomTypes.h>
#include <Cry_Math.h>
#include <Random.h>
#include <AzCore/Casting/numeric_cast.h>

BASIC_TYPE_INFO(CCryName)

#define PARTICLE_PARAMS_DEFAULT_BOUNDING_VOLUME 5.f
#define PARTICLE_PARAMS_DEFAULT_BLUR_STRENGTH 20.f
#define PARTICLE_PARAMS_WIDTH_TO_HALF_WIDTH 0.5f
#define PARTICLE_PARAMS_MAX_COUNT_CPU 20000 
#define PARTICLE_PARAMS_MAX_MAINTAIN_DENSITY 10000 
#define PARTICLE_PARAMS_MAX_COUNT_GPU 1048576 

///////////////////////////////////////////////////////////////////////

#define fHUGE           1e9f        // Use for practically infinite values, to simplify comparisons.
// 1e9 s > 30 years, 1e9 m > earth-moon distance.
// Convertible to int32.

// Convert certain parameters where zero denotes an essentially infinite value.
ILINE float ZeroIsHuge(float f)
{
    return if_pos_else(f, f, fHUGE);
}

// For accessing specific information from variable parameters,
// use VMIN, VMAX (from CryMath.h), and VRANDOM (defined here)
enum type_random
{
    VRANDOM
};

///////////////////////////////////////////////////////////////////////
// Trinary enum, encodes yes/no/both state

// Original names, compatible with serialization
DEFINE_ENUM_VALS(ETrinaryNames, uint8,
    If_False = 1,
    If_True,
    Both
    )

struct ETrinary
    : ETrinaryNames
{
    // Initialized with bool value, or Both (default)
    ETrinary(ETrinaryNames::E e = Both)
        : ETrinaryNames(e) {}
    ETrinary(bool b)
        : ETrinaryNames(ETrinaryNames::E(1 << uint8(b))) {}

    // Test against ETrinary, or directly against bool
    uint8 operator *(ETrinary t) const
    {
        return aznumeric_caster(+*this & + t);
    }
    uint8 operator *(bool b) const
    {
        return +*this & (1 << uint8(b));
    }
};

///////////////////////////////////////////////////////////////////////
// Pseudo-random number generation, from a key.

class CChaosKey
{
public:
    // Initialize with an int.
    explicit inline CChaosKey(uint32 uSeed)
        : m_Key(uSeed) {}

    explicit inline CChaosKey(float fSeed)
        : m_Key((uint32)(fSeed * float(0xFFFFFFFF))) {}

    explicit inline CChaosKey(void const* ptr)
        : m_Key(uint32(size_t(ptr))) {}

    CChaosKey Jumble(CChaosKey key2) const
    {
        return CChaosKey(Jumble(m_Key ^ key2.m_Key));
    }
    CChaosKey Jumble(void const* ptr) const
    {
        return Jumble(CChaosKey(ptr));
    }

    // Scale input range.
    inline float operator *(float fRange) const
    {
        return (float)m_Key / float(0xFFFFFFFF) * fRange;
    }
    inline uint32 operator *(uint32 nRange) const
    {
        return m_Key % nRange;
    }
    inline uint16 operator *(uint16 nRange) const
    {
        return m_Key % nRange;
    }

    uint32 GetKey() const
    {
        return m_Key;
    }

    // Jumble with a range variable to produce a random value.
    template<class T>
    inline T Random(T const& Range) const
    {
        return T(Jumble(CChaosKey(&Range)) * Range);
    }

private:
    uint32  m_Key;

    static inline uint32 Jumble(uint32 key)
    {
        key += ~rot_left(key, 15);
        key ^= rot_right(key, 10);
        key += rot_left(key, 3);
        key ^= rot_right(key, 6);
        key += ~rot_left(key, 11);
        key ^= rot_right(key, 16);
        return key;
    }

    static inline uint32 rot_left(uint32 u, int n)
    {
        return (u << n) | (u >> (32 - n));
    }
    static inline uint32 rot_right(uint32 u, int n)
    {
        return (u >> n) | (u << (32 - n));
    }
};

//////////////////////////////////////////////////////////////////////////
//
// Float storage
//

typedef TRangedType<float>                  SFloat;
typedef TRangedType<float, 0>                UFloat;
typedef TRangedType<float, 0, 1>          UnitFloat;
typedef TRangedType<float, 0, 2>          Unit2Float;
typedef TRangedType<float, 0, 4>          Unit4Float;
typedef TRangedType<float, -180, 180> SAngle;
typedef TRangedType<float, 0, 180>        UHalfAngle;
typedef TRangedType<float, 0, 360>        UFullAngle;
typedef TRangedType<float, 0, PARTICLE_PARAMS_MAX_MAINTAIN_DENSITY> UFloatMaintainDensity;
   
typedef TSmall<bool>                                TSmallBool;
typedef TSmall<bool, uint8, 0, 1>          TSmallBoolTrue;
typedef TSmall<uint, uint8, 1>                PosInt8;

template<class T>
struct TStorageTraits
{
    typedef float               TValue;
    typedef UnitFloat8  TMod;
    typedef UnitFloat       TRandom;
};

template<>
struct TStorageTraits<SFloat>
{
    typedef float               TValue;
    typedef UnitFloat8  TMod;
    typedef Unit2Float  TRandom;
};

template<>
struct TStorageTraits<SAngle>
{
    typedef float               TValue;
    typedef UnitFloat8  TMod;
    typedef Unit2Float  TRandom;
};


template<class TFixed>
bool RandomActivate(const TFixed& chance)
{
    return cry_random(0, chance.GetMaxStore() - 1) < chance.GetStore();
}

typedef Vec3_tpl<SFloat> Vec3S;
typedef Vec3_tpl<UFloat> Vec3U;

//////////////////////////////////////////////////////////////////////////
//
// Color specialisations.

template<class T>
struct Color3
    : Vec3_tpl<T>
{
    Color3(T v = T(0))
        : Vec3_tpl<T>(v) {}

    Color3(T r, T g, T b)
        : Vec3_tpl<T>(r, g, b) {}

    Color3(Vec3_tpl<T> const& v)
        : Vec3_tpl<T>(v) {}

    template<class T2>
    Color3(Vec3_tpl<T2> const& c)
        : Vec3_tpl<T>(c) {}

    operator ColorF() const
    {
        return ColorF(this->x, this->y, this->z, 1.f);
    }

    // Implement color multiplication.
    Color3& operator *=(Color3 const& c)
    { this->x *= c.x; this->y *= c.y; this->z *= c.z; return *this; }
};

template<class T>
Color3<T> operator *(Color3<T> const& c, Color3<T> const& d)
{ return Color3<T>(c.x * d.x, c.y * d.y, c.z * d.z); }

typedef Color3<float> Color3F;
typedef Color3<UnitFloat8> Color3B;

class RandomColor
    : public UnitFloat8
{
public:
    inline RandomColor(float f = 0.f, bool bRandomHue = false)
        : UnitFloat8(f)
        , m_bRandomHue(bRandomHue)
    {}

    Color3F GetRandom() const
    {
        if (m_bRandomHue)
        {
            ColorB clr(cry_random_uint32());
            float fScale = float(*this) / 255.f;
            return Color3F(clr.r * fScale, clr.g * fScale, clr.b * fScale);
        }
        else
        {
            return Color3F(cry_random(0.0f, float(*this)));
        }
    }

    AUTO_STRUCT_INFO

protected:
    TSmallBool  m_bRandomHue;
};


template<>
struct TStorageTraits<Color3F>
{
    typedef Color3F                         TValue;
    typedef Color3<UnitFloat8>  TMod;
    typedef RandomColor                 TRandom;
};

///////////////////////////////////////////////////////////////////////
//
// Spline implementation class.
//

template<>
inline const CTypeInfo& TypeInfo(ISplineInterpolator**)
{
    static CTypeInfo Info("ISplineInterpolator*", sizeof(void*), alignof(void*));
    return Info;
}

template<class S>
class TCurve
    : public spline::OptSpline< typename TStorageTraits<S>::TValue >
{
    typedef TCurve<S> TThis;
    typedef typename TStorageTraits<S>::TValue T;
    typedef typename TStorageTraits<S>::TMod TMod;
    typedef spline::OptSpline<T> super_type;

public:
    using_type(super_type, source_spline);
    using_type(super_type, key_type);

    using super_type::num_keys;
    using super_type::key;
    using super_type::empty;
    using super_type::min_value;
    using super_type::interpolate;

    // Implement serialization.
    string ToString(FToString flags = 0) const;
    bool FromString(cstr str, FFromString flags = 0);

    // Spline interface.
    ILINE T GetValue(float fTime) const
    {
        T val;
        interpolate(fTime, val);
        return val;
    }
    T GetMinValue() const
    {
        T val;
        min_value(val);
        return val;
    }
    bool IsIdentity() const
    {
        return GetMinValue() == T(1.f);
    }

    // Access operators
    T operator ()(float fTime) const
    {
        T val;
        interpolate(fTime, val);
        return val;
    }

    T operator ()(type_max) const
    {
        return T(1);
    }

    T operator ()(type_min) const
    {
        T val;
        min_value(val);
        return val;
    }
    
    bool operator == (const TThis& o) const
    {
        return super_type::operator==(o);
    }

    struct CCustomInfo;
    CUSTOM_STRUCT_INFO(CCustomInfo)
};

///////////////////////////////////////////////////////////////////////
//
// Composite parameter types, incorporating base value, randomness, and lifetime curves.
//

template<class S>
struct TVarParam
    : S
{
    typedef typename TStorageTraits<S>::TValue T;

    // Define random variation capability.
    using_type(TStorageTraits<S>, TRandom);

    struct SRandom
        : TRandom
    {
        SRandom(TRandom r = TRandom(0))
            : TRandom(r) {}

        const TRandom& Range() const
        { return static_cast<const TRandom&>(*this); }

        // Define access operators
        T operator () (type_max) const
        { return T(1); }

        T operator () (type_min) const
        { return T(1) - Range(); }

        T operator () (float fInterp) const
        { return T(1) + T(Range() * (fInterp - 1.f)); }

        T operator () (CChaosKey key) const
        {
            if (!!Range())
            {
                return T(1) - T(key.Random(Range()));
            }
            else
            {
                return T(1);
            }
        }

        T operator () (type_random) const
        {
            if (!!Range())
            {
                return T(1) - T(ParamRandom(Range()));
            }
            else
            {
                return T(1);
            }
        }

    private:
        static Color3F ParamRandom(RandomColor const& rc)
        {
            return rc.GetRandom();
        }

        static float ParamRandom(float v)
        {
            return cry_random(0.0f, v);
        }
    };

    // Construction.
    TVarParam()
        : S()
    {
    }

    TVarParam(const T& tBase)
        : S(tBase)
    {
    }

    void Set(const T& tBase)
    {
        Base() = tBase;
    }

    void Set(const T& tBase, const TRandom& tRan)
    {
        Base() = tBase;
        m_Random = tRan;
    }

    //
    // Value extraction.
    //
    S& Base()
    {
        return static_cast<S&>(*this);
    }
    const S& Base() const
    {
        return static_cast<const S&>(*this);
    }

    ILINE bool operator !() const
    {
        return Base() == S(0);
    }

    // Value access is base value modified by random function
    template<class R>
    T operator ()(R r) const
    {
        return Base() * m_Random(r);
    }

    // Legacy helper
    ILINE T GetMaxValue() const
    {
        return Base();
    }
    ILINE T GetMinValue() const
    {
        return Base() - Base() * float(m_Random);
    }

    ILINE TRandom GetRandomRange() const
    {
        return m_Random;
    }

    ILINE T GetVarMod() const
    {
        T val = T(1.f);
        if (m_Random)
        {
            T ran(Random(m_Random));
            ran *= val;
            val -= ran;
        }
        return val;
    }

    ILINE T GetVarValue() const
    {
        return GetVarMod() * T(Base());
    }

    ILINE T GetValue(CChaosKey key) const
    {
        T val = Base();
        if (!!m_Random)
        {
            val -= key.Random(m_Random) * val;
        }
        return val;
    }

    ILINE T GetValue(float fRange) const
    {
        T val = Base();
        val *= (1.f - (1.f - fRange) * m_Random);
        return val;
    }

    ILINE T GetValueFromMod(T val) const
    {
        return T(Base()) * val;
    }

    AUTO_STRUCT_INFO

protected:

    SRandom         m_Random;           // Random variation, multiplicative.
};

///////////////////////////////////////////////////////////////////////

template<class S>
struct TVarEParam
    : TVarParam<S>
{
    typedef TVarParam<S> TSuper;
    typedef typename TStorageTraits<S>::TValue  T;

    // Construction.
    ILINE TVarEParam()
    {
    }

    ILINE TVarEParam(const T& tBase)
        : TSuper(tBase)
    {
    }

    //
    // Value extraction.
    //

    T GetMinValue() const
    {
        T val = TVarParam<S>::GetMinValue();
        val *=  m_EmitterStrength.GetMinValue();
        return val;
    }

    bool IsConstant() const
    {
        return  m_EmitterStrength.IsIdentity();
    }

    ILINE T GetVarMod(float fEStrength) const
    {
        T val = T(1.f);
        if (m_Random)
        {
            val -= Random(m_Random);
        }
        val *=  m_EmitterStrength.GetValue(fEStrength);
        return val;
    }

    T operator ()(type_max) const
    {
        return TSuper::operator()(VMAX);
    }

    T operator ()(type_min) const
    {
        return TSuper::operator()(VMIN) *  m_EmitterStrength(VMIN);
    }

    template<class R, class E>
    T operator ()(R r, E e) const
    {
        return TSuper::operator()(r) *  m_EmitterStrength(e);
    }

    ILINE T GetVarValue(float fEStrength) const
    {
        return GetVarMod(fEStrength) * T(Base());
    }

    ILINE T GetValue(float fEStrength, CChaosKey keyRan) const
    {
        T val = Base();
        if (!!m_Random)
        {
            val -= keyRan.Random(m_Random) * val;
        }
        val *=  m_EmitterStrength.GetValue(fEStrength);
        return val;
    }

    ILINE T GetValue(float fEStrength, float fRange) const
    {
        T val = Base();
        val *= (1.f - (1.f - fRange) * m_Random);
        val *=  m_EmitterStrength.GetValue(fEStrength);
        return val;
    }

    void SetEmitterStrength(const TCurve<S>& tStrengthOverEmitterLife)
    {
         m_EmitterStrength = tStrengthOverEmitterLife;
    }

    const TCurve<S>& GetEmitterStrength() const
    {
        return  m_EmitterStrength;
    }

    AUTO_STRUCT_INFO

protected:
    TCurve<S>        m_EmitterStrength;

    // Dependent name nonsense.
    using TSuper::Base;
    using TSuper::m_Random;
};

///////////////////////////////////////////////////////////////////////
template<class S>
struct TVarEPParam
    : TVarEParam<S>
{
    typedef TVarEParam<S> TSuper;
    typedef typename TStorageTraits<S>::TValue  T;

    // Construction.
    TVarEPParam()
    {}

    TVarEPParam(const T& tBase)
        : TSuper(tBase)
    {}

    //
    // Value extraction.
    //

    T operator ()(type_max) const
    {
        return TSuper::operator()(VMAX);
    }

    T operator ()(type_min) const
    {
        return TSuper::operator()(VMIN) * m_ParticleAge(VMIN);
    }

    template<class R, class E, class P>
    T operator ()(R r, E e, P p) const
    {
        return TSuper::operator()(r, e) * m_ParticleAge(p);
    }

    // Note this hides TVarEParam::IsConstant(). This should be fine because params are only accessed directly, 
    // never through base pointers. We could make TVarEParam::IsConstant() virtual but there isn't a good reason
    // to do that.
    bool IsConstant() const
    {
        return  m_ParticleAge.IsIdentity() && TSuper::IsConstant();
    }

    // Additional helpers
    T GetVarMod(float fEStrength) const
    {
        return m_Random(VRANDOM) * m_EmitterStrength(fEStrength);
    }

    ILINE T GetValueFromBase(T val, float fParticleAge) const
    {
        return val * m_ParticleAge(fParticleAge);
    }

    ILINE T GetValueFromMod(T val, float fParticleAge) const
    {
        return T(Base()) * val * m_ParticleAge(fParticleAge);
    }

    void SetParticleAge(const TCurve<S>& tParticleAge)
    {
        m_ParticleAge = tParticleAge;
    }

    const TCurve<S>& GetParticleAge() const
    {
        return m_ParticleAge;
    }

    using TSuper::GetValueFromMod;

    AUTO_STRUCT_INFO

    // Dependent name nonsense.
    using TSuper::Base;

protected:

    TCurve<S>   m_ParticleAge;

    // Dependent name nonsense.
    using TSuper::m_Random;
    using TSuper::m_EmitterStrength;
};


template <class T>
struct Vec3_TVarEPParam
{
    TVarEPParam<T> fX;  // size along X axis
    TVarEPParam<T> fY;  // size along Y axis
    TVarEPParam<T> fZ;  // size along Z axis
    Vec3_TVarEPParam<T>()
    : fX(0)
    , fY(0)
    , fZ(0)
    {
    }

    Vec3_tpl<T> GetMaxVector() const
    {
        return Vec3_tpl<T>(fX.GetMaxValue(), fY.GetMaxValue(), fZ.GetMaxValue());
    }

    Vec3_tpl<T> GetMinVector() const
    {
        return Vec3_tpl<T>(fX.GetMinValue(), fY.GetMinValue(), fZ.GetMinValue());
    }

    Vec3_tpl<T> GetVector(float e, float p) const
    {
        return Vec3_tpl<T>(fX(VRANDOM, e, p), fY(VRANDOM, e, p), fZ(VRANDOM, e, p));
    }

    Vec3_tpl<T> GetVector(float r, float e, float p) const
    {
        return Vec3_tpl<T>(fX(r, e, p), fY(r, e, p), fZ(r, e, p));
    }

    Vec3_tpl<T> GetVector() const
    {
        return Vec3_tpl<T>(fX, fY, fZ);
    }

    Vec3_tpl<T> GetVarModVector(float e) const
    {
        return Vec3_tpl<T>(fX.GetVarMod(e), fY.GetVarMod(e), fZ.GetVarMod(e));
    }

    Vec3_tpl<T> GetRandomOffset() const
    {
        return Vec3_tpl<T>(fX.GetRandomRange(), fY.GetRandomRange(), fZ.GetRandomRange());
    }
    AABB GetBBox() const
    {
        AABB box(0.f);
        box.Expand(GetMaxVector());
        return box;
    }

    //Vector operations for backwards compatibility
    ILINE Vec3_tpl<T> operator * (T k) const
    {
        Vec3_tpl<T>(fX, fY, fZ) * k;
    }
    ILINE Vec3_tpl<T> operator / (T k) const
    {
        k = (T)1.0 / k;
        return Vec3_tpl<T>(fX, fY, fZ) * k;
    }

    ILINE Vec3_tpl<T>& operator *= (T k)
    {
        fX *= k;
        fY *= k;
        fZ *= k;
        return *this;
    }
    ILINE Vec3_tpl<T>& operator /= (T k)
    {
        k = (T)1.0 / k;
        fX *= k;
        fY *= k;
        fZ *= k;
        return *this;
    }

    ILINE Vec3_tpl<T> operator - (void) const
    {
        return Vec3_tpl<T>(-fX, -fY, -fZ);
    }


    ILINE Vec3_tpl<T> operator + (const Vec3_tpl<T>& k) const
    {
        return Vec3_tpl<T>(fX + k.x, fY + k.y, fZ + k.z);
    }

    ILINE Vec3_TVarEPParam<T> operator + (const Vec3_TVarEPParam<T>& k)
    {
        Vec3_TVarEPParam<T> result;
        result.fX = fX + k.fX;
        result.fY = fY + k.fY;
        result.fZ = fZ + k.fZ;
        return result;
    }

    ILINE Vec3_TVarEPParam<T> operator = (const Vec3_TVarEPParam<T>& k)
    {
        fX = k.fX;
        fY = k.fY;
        fZ = k.fZ;

        return *this;
    }

    AUTO_STRUCT_INFO
};
template <class T>
struct Vec3_TVarEParam
    : Vec3_tpl < T >
{
    TVarEParam<T> fX;  // size along X axis
    TVarEParam<T> fY;  // size along Y axis
    TVarEParam<T> fZ;  // size along Z axis
    Vec3_TVarEParam<T>()
    : fX(0)
    , fY(0)
    , fZ(0)
    {
    }

    Vec3_TVarEParam<T>( Vec3_tpl<T> baseValue )
    : Vec3_tpl<T>(baseValue)
    , fX(0)
    , fY(0)
    , fZ(0)
    {
    }

    Vec3_tpl<T> GetMaxVector() const
    {
        return Vec3_tpl<T>(fX.GetMaxValue(), fY.GetMaxValue(), fZ.GetMaxValue());
    }
    Vec3_tpl<T> GetMinVector() const
    {
        return Vec3_tpl<T>(fX.GetMinValue(), fY.GetMinValue(), fZ.GetMinValue());
    }
    Vec3_tpl<T> GetVector(float e) const
    {
        return Vec3_tpl<T>(fX(VRANDOM, e), fY(VRANDOM, e), fZ(VRANDOM, e));
    }
    Vec3_tpl<T> GetVector() const
    {
        return Vec3_tpl<T>(fX, fY, fZ);
    }

    //Vector operations for backwards compatibility
    ILINE Vec3_tpl<T> operator * (T k) const
    {
        Vec3_tpl<T>(fX, fY, fZ) * k;
    }
    ILINE Vec3_tpl<T> operator / (T k) const
    {
        k = (T)1.0 / k;
        return Vec3_tpl<T>(fX, fY, fZ) * k;
    }

    ILINE Vec3_tpl<T>& operator *= (T k)
    {
        fX *= k;
        fY *= k;
        fZ *= k;
        return *this;
    }
    ILINE Vec3_tpl<T>& operator /= (T k)
    {
        k = (T)1.0 / k;
        fX *= k;
        fY *= k;
        fZ *= k;
        return *this;
    }

    ILINE Vec3_tpl<T> operator - (void) const
    {
        return Vec3_tpl<T>(-fX, -fY, -fZ);
    }


    ILINE Vec3_TVarEParam<T> operator + (const Vec3_tpl<T>& k) const
    {
        Vec3_TVarEParam<T> result;
        result.fX = fX + k.fX;
        result.fX.SetEmitterStrength(fX.GetEmitterStrength());
        result.fY = fY + k.fY;
        result.fY.SetEmitterStrength(fY.GetEmitterStrength());
        result.fZ = fZ + k.fZ;
        result.fZ.SetEmitterStrength(fZ.GetEmitterStrength());
        return result;
    }

    ILINE Vec3_TVarEParam<T> operator + (const Vec3_TVarEParam<T>& k) const
    {
        Vec3_TVarEParam<T> result;
        result.fX = fX + k.fX;
        result.fX.SetEmitterStrength(fX.GetEmitterStrength());
        result.fY = fY + k.fY;
        result.fY.SetEmitterStrength(fY.GetEmitterStrength());
        result.fZ = fZ + k.fZ;
        result.fZ.SetEmitterStrength(fZ.GetEmitterStrength());
        return result;
    }

    ILINE Vec3_TVarEParam<T> operator = (const Vec3_TVarEParam<T>& k)
    {
        fX = k.fX;
        fX.SetEmitterStrength(k.fX.GetEmitterStrength());
        fY = k.fY;
        fY.SetEmitterStrength(k.fY.GetEmitterStrength());
        fZ = k.fZ;
        fZ.SetEmitterStrength(k.fZ.GetEmitterStrength());
        return *this;
    }

    AUTO_STRUCT_INFO
};

typedef Vec3_TVarEPParam < UFloat > Vec3_TVarEPParam_UFloat;
typedef Vec3_TVarEPParam < SFloat > Vec3_TVarEPParam_SFloat;

typedef Vec3_TVarEParam < UFloat > Vec3_TVarEParam_UFloat;
typedef Vec3_TVarEParam < SFloat > Vec3_TVarEParam_SFloat;


template <typename C>
inline C conditionalLerp(C const& c1, C const& c2, float w, bool useLerp = true)
{
    w = useLerp ? w : 1.0f;
    C clerp;
    clerp = c1 * w + (1.0f - w) * c2;
    return clerp;
}

template <>
inline Color3F conditionalLerp<Color3F>(Color3F const& c1, Color3F const& c2, float w, bool useLerp)
{
    w = useLerp ? w : 1.0f;
    Color3F clerp;
    clerp.x = c1.x * w + (1.0f - w) * c2.x;
    clerp.y = c1.y * w + (1.0f - w) * c2.y;
    clerp.z = c1.z * w + (1.0f - w) * c2.z;
    return clerp;
}

template<class S>
struct TVarEPParamRandLerp
    : TVarEPParam<S>
{
    typedef TVarEPParam<S> TSuper;
    typedef typename TStorageTraits<S>::TValue  T;

    // Construction.
    TVarEPParamRandLerp()
    {}

    TVarEPParamRandLerp(const T& tBase)
        : TSuper(tBase)
    {}

    // Legacy helper
    ILINE T GetMaxValue(float lerp) const
    {
        return GetVarValue(lerp);
    }
    ILINE T GetMinValue(float lerp) const
    {
        float v = GetVarValue(lerp);
        return v - v * float(m_Random);
    }

    ILINE float GetLerp() const
    {
        return cry_frand();
    }

    T operator ()(type_max) const
    {
        return TSuper::operator()(VMAX);
    }

    T operator ()(type_min) const
    {
        S minValue = TSuper::GetMinValue();
        return MIN(minValue, m_Color) * MIN(m_EmitterStrength(VMIN), m_EmitterStrength2(VMIN)) * MIN(m_ParticleAge(VMIN), m_ParticleAge2(VMIN));
    }

    template<class R>
    T operator ()(R r, float lerp) const
    {
        return conditionalLerp<S>(Base(), m_Color, lerp, !m_bRandomLerpColor /*use random if random between colors is false*/) * m_Random(r);
    }

    template<class R, class E>
    T operator ()(R r, E e, float lerp) const
    {
        return operator()(r, lerp) * conditionalLerp<S>(m_EmitterStrength.GetValue(e), m_EmitterStrength2.GetValue(e), lerp, m_bRandomLerpStrength);
    }

    template<class R, class E, class P>
    T operator ()(R r, E e, P p, float lerp) const
    {
        return operator()(r, e, lerp) * conditionalLerp<S>(m_ParticleAge(p), m_ParticleAge2(p), lerp, m_bRandomLerpAge);
    }

    // Additional helpers
    ILINE T GetVarValue(float lerp) const
    {
        return T(conditionalLerp<S>(Base(), m_Color, lerp, m_bRandomLerpColor));
    }

    ILINE T GetVarValue(float fEStrength, float lerp) const
    {
        return GetVarMod(fEStrength, lerp) * T(conditionalLerp<S>(Base(), m_Color, lerp, m_bRandomLerpColor));
    }

    ILINE T GetValue(float fEStrength, CChaosKey keyRan, float lerp) const
    {
        T val = conditionalLerp<S>(Base(), m_Color, lerp, m_bRandomLerpColor);
        if (!!m_Random)
        {
            val -= keyRan.Random(m_Random) * val;
        }
        val *= conditionalLerp<S>(m_EmitterStrength.GetValue(fEStrength), m_EmitterStrength2.GetValue(fEStrength), lerp, m_bRandomLerpStrength);
        return val;
    }

    ILINE T GetValue(float fEStrength, float fRange, float lerp) const
    {
        T val = conditionalLerp<S>(Base(), m_Color, lerp, m_bRandomLerpColor);
        val *= (1.f - (1.f - fRange) * m_Random);
        val *= conditionalLerp<S>(m_EmitterStrength.GetValue(fEStrength), m_EmitterStrength2.GetValue(fEStrength), lerp, m_bRandomLerpStrength);
        return val;
    }

    T GetVarMod(float fEStrength, float lerp) const
    {
        S c1 = m_EmitterStrength(fEStrength);
        S c2 = m_EmitterStrength2(fEStrength);
        return m_Random(VRANDOM) * conditionalLerp<S>(c1, c2, lerp, m_bRandomLerpStrength);
    }

    ILINE T GetValueFromBase(T val, float fParticleAge, float lerp) const
    {
        return val * conditionalLerp<T>(m_ParticleAge(fParticleAge), m_ParticleAge2(fParticleAge), lerp, m_bRandomLerpAge);
    }

    ILINE T GetValueFromMod(T val, float fParticleAge, float lerp) const
    {
        return T(conditionalLerp<S>(Base(), m_Color, lerp, m_bRandomLerpColor)) * val * conditionalLerp<S>(m_ParticleAge(fParticleAge), m_ParticleAge2(fParticleAge), lerp, m_bRandomLerpAge);
    }

    ILINE T GetLerpMultiplier(float lerp) const
    {
        return T(conditionalLerp<S>(Base(), m_Color, lerp, m_bRandomLerpColor)) / GetMaxBaseLerp();
    }
    ILINE T GetMaxBaseLerp() const
    {
        const T& base = Base();
        return MAX(base, m_Color * m_bRandomLerpColor ? 1.0f : 0.0f);
    }

    ILINE const TCurve<S>& GetSecondaryEmitterStrength() const
    {
        return m_EmitterStrength2;
    }

    ILINE const TCurve<S>& GetSecondaryParticleAge() const
    {
        return m_ParticleAge2;
    }

    ILINE const TSmallBool& GetRandomLerpColor() const
    {
        return m_bRandomLerpColor;
    }

    ILINE const TSmallBool& GetRandomLerpStrength() const
    {
        return m_bRandomLerpStrength;
    }

    ILINE const TSmallBool& GetRandomLerpAge() const
    {
        return m_bRandomLerpAge;
    }

    ILINE const S& GetSecondValue() const
    {
        return m_Color;
    }

    AUTO_STRUCT_INFO
protected:
    S m_Color;
    // m_EmitterStrength2 and m_ParticleAge2 are secondary values set in a curve
    // typical usage is interpolation between primary and secondary values
    TCurve<S>       m_EmitterStrength2;
    TCurve<S>       m_ParticleAge2;
    TSmallBool  m_bRandomLerpColor;
    TSmallBool  m_bRandomLerpStrength;
    TSmallBool  m_bRandomLerpAge;

    using TSuper::Base;
    using TSuper::m_ParticleAge;
    using TSuper::m_Random;
    using TSuper::m_EmitterStrength;
};

///////////////////////////////////////////////////////////////////////
template<class S>
struct TRangeParam
{
    S       Min;
    S       Max;

    TRangeParam()
    {}
    TRangeParam(S _min, S _max)
        : Min(_min)
        , Max(_max) {}

    S Interp(float t) const
    { return Min * (1.f - t) + Max * t; }

    AUTO_STRUCT_INFO
};

///////////////////////////////////////////////////////////////////////
// Special type enums

struct CSurfaceTypeIndex
{
    uint16  nIndex;

    STRUCT_INFO
};

struct CAZPhysicsCollisionLayerIndex
{
    AZ::u8 layer;

    STRUCT_INFO
};

struct CAZPhysicsCollisionGroupIndex
{
    AZ::Uuid group;

    STRUCT_INFO
};

struct CAZPhysicsMaterialIndex
{
    AZ::Uuid material;

    STRUCT_INFO
};

///////////////////////////////////////////////////////////////////////
//! Particle system parameters
//! Note: If you add a Vec3_tpl (or any object that owns a Vec3_tpl) they must be added to the initialization list.
//! This is due to the default constructors executing after ZeroInit, and Vec3_tpl's default ctor initializes its values to NAN in _DEBUG
struct ParticleParams
    : ZeroInit<ParticleParams>
{
    // <Group=Emitter>
    string sComment;
    TSmallBool bEnabled;                                    // Set false to disable this effect

    DEFINE_ENUM(EEmitterType,
        CPU,
        GPU
        )
    EEmitterType eEmitterType;                          // Emitter type

    DEFINE_ENUM(EEmitterShapeType,
        ANGLE,
        POINT,
        SPHERE,
        CIRCLE,
        BOX,
        TRAIL,
        BEAM
    )
    EEmitterShapeType eEmitterShape;                            // Emitter shape type

    DEFINE_ENUM(EEmitterGPUShapeType,
        ANGLE,
        POINT,
        SPHERE,
        CIRCLE,
        BOX
    )
    EEmitterGPUShapeType eEmitterGpuShape;                            // Emitter GPU shape type //only necessary when this enum's values != eEmitterShape



    DEFINE_ENUM(EInheritance,
        Standard,
        System,
        Parent
        )
    EInheritance eInheritance;                          // Source of ParticleParams used as base for this effect (for serialization, display, etc)

    DEFINE_ENUM(ESpawn,
        Direct,
        ParentStart,
        ParentCollide,
        ParentDeath
        )
    ESpawn eSpawnIndirection;                               // Direct: spawn from emitter location; else spawn from each particle in parent emitter
    DEFINE_ENUM(EGPUSpawn,
        Direct,
        ParentDeath)
    EGPUSpawn eGPUSpawnIndirection;

    TVarEParam<UFloat> fCount;                      // Number of particles alive at once
    TVarEParam<UFloat> fBeamCount;                  // Number of Beams alive at once

    typedef TRangedType<float, 0, PARTICLE_PARAMS_MAX_MAINTAIN_DENSITY> UFloatMaintainDensity;
    struct SMaintainDensity
        : UFloatMaintainDensity
    {
        UFloat fReduceLifeTime;                         //
        UFloat fReduceAlpha;                                // <SoftMax=1> Reduce alpha inversely to count increase.
        UFloat fReduceSize;                                 //
        AUTO_STRUCT_INFO
    } fMaintainDensity;                                     // <SoftMax=1> Increase count when emitter moves to maintain spatial density.

    // <Group=LOD> - Vera Confetti
    UFloat fBlendIn;                                    // Level of detail, blend in
    UFloat fBlendOut;                                   // Level of detail, blend out
    UFloat fOverlap;                                    // Level of detail, overlap
    TSmallBool bRemovedFromLod;                         // Level of detail, signifies that the particle has been removed from the lod. (we keep particles in the lod tree, to keep the tree intact)

    // <Group=Timing>
    TSmallBool bContinuous;                             // Emit particles gradually until Count reached (rate = Count / ParticleLifeTime)
    TVarParam<SFloat> fSpawnDelay;              // Delay the emitter start time by this value
    TVarParam<UFloat> fEmitterLifeTime;     // Lifetime of the emitter, 0 if infinite. Always emits at least Count particles
    TVarParam<SFloat> fPulsePeriod;             // Time between auto-restarts of emitter; 0 if never
    TVarEParam<UFloat> fParticleLifeTime;   // Lifetime of particles, 0 if indefinite (die with emitter)
    TSmallBool bRemainWhileVisible;             // Particles will only die when not rendered (by any viewport)

    struct BoundingVolume
        : public TSmallBool                  // Is bounding volume enabled
    {
        TVarEParam<UFloat> fX;  // size along X axis
        TVarEParam<UFloat> fY;  // size along Y axis
        TVarEParam<UFloat> fZ;  // size along Z axis
        BoundingVolume()
            : fX(PARTICLE_PARAMS_DEFAULT_BOUNDING_VOLUME)
            , fY(PARTICLE_PARAMS_DEFAULT_BOUNDING_VOLUME)
            , fZ(PARTICLE_PARAMS_DEFAULT_BOUNDING_VOLUME)
        {
        }

        Vec3 GetMaxVector() const
        {
            return Vec3(fX.GetMaxValue(), fY.GetMaxValue(), fZ.GetMaxValue());
        }
        Vec3 GetMinVector() const
        {
            return Vec3(fX.GetMinValue(), fY.GetMinValue(), fZ.GetMinValue());
        }
        Vec3 GetVector(float e) const
        {
            return Vec3(fX(VRANDOM, e), fY(VRANDOM, e), fZ(VRANDOM, e));
        }
        AABB GetBBox() const
        {
            AABB box(0.f);
            box.Expand(GetMaxVector());
            return box;
        }

        AUTO_STRUCT_INFO
    };

    // <Group=Location>
    Vec3S vSpawnPosOffset;                          // Spawn offset from the emitter position
    Vec3U vSpawnPosRandomOffset;                    // Spawn offset from the emitter position
    UnitFloat fOffsetRoundness;                     // Fraction of emit volume corners to round: 0 = box, 1 = ellipsoid
    TVarEParam<UnitFloat> fSpawnIncrement;
    EGeomType eAttachType;                              // Which geometry to use for attached entity
    EGeomForm eAttachForm;                              // Which aspect of attached geometry to emit from

    // <Group=Angles>
    TVarEParam<UHalfAngle> fFocusAngle;     // Angle to vary focus from default (Z axis), for variation
    TVarEParam<SFloat> fFocusAzimuth;           // <SoftMax=360> Angle to rotate focus about default, for variation. 0 = Y axis
    TVarEParam<UnitFloat> fFocusCameraDir;// Rotate emitter focus partially or fully to face camera
    TSmallBool bFocusGravityDir;                    // Uses negative gravity dir, rather than emitter Y, as focus dir
    TSmallBool bFocusRotatesEmitter;            // Focus rotation affects offset and particle orientation; else affects just emission direction
    TSmallBool bEmitOffsetDir;                      // Default emission direction parallel to emission offset from origin
    TVarEParam<UHalfAngle> fEmitAngle;      // Angle from focus dir (emitter Y), in degrees. RandomVar determines min angle

    DEFINE_ENUM(EFacing,
        Camera,
        CameraX,
        Free,
        Horizontal,
        Velocity,
        Water,
        Terrain,
        Decal,
        Shape
        )
    EFacing eFacing;                                        // Orientation of particle face for GPU particles

    DEFINE_ENUM(EFacingGpu,
        Camera,
        Free,
        Shape
        )
    EFacingGpu eFacingGpu;
    TSmallBool bOrientToVelocity;                   // Particle X axis aligned to velocity direction
    UnitFloat fCurvature;                                   // For Facing=Camera, fraction that normals are curved to a spherical shape

    // <Group=Appearance>
    DEFINE_ENUM_VALS(EBlend, uint8,
        AlphaBased = OS_ALPHA_BLEND,
        Additive = OS_ADD_BLEND,
        Multiplicative = OS_MULTIPLY_BLEND,
        Opaque = 0,
        _ColorBased = OS_ADD_BLEND,
        _None = 0
        )
    EBlend eBlendType;                                          // Blend rendering type
    CCryName sTexture;                                      // Texture asset for sprite
    CCryName sNormalMap;                                // Normal asset for sprite
    CCryName sGlowMap;                              // Glow asset for sprite
    CCryName sMaterial;                                     // Material (overrides texture)


    DEFINE_ENUM(ESortMethod,
        None,
        Bitonic,
        OddEven
        )
    ESortMethod eSortMethod;                                // Emitter type
    UnitFloat fSortConvergancePerFrame;

    struct STextureTiling
    {
        PosInt8 nTilesX, nTilesY;           // Number of tiles texture is split into
        uint8       nFirstTile;                     // First (or only) tile to use
        PosInt8 nVariantCount;              // Number of randomly selectable tiles; 0 or 1 if no variation
        PosInt8 nAnimFramesCount;           // Number of tiles (frames) of animation; 0 or 1 if no animation
        DEFINE_ENUM(EAnimationCycle,
            Once,
            Loop,
            Mirror
            )
        EAnimationCycle eAnimCycle;         // How animation cycles
        TSmallBool bAnimBlend;              // Blend textures between frames
        UnitFloat8 fHorizontalFlipChance;       // Chance each particle will flip along U axis
        UnitFloat8 fVerticalFlipChance;     // Chance each particle will flip along V axis
        UFloat  fAnimFramerate;             // <SoftMax=60> Tex framerate; 0 = 1 cycle / particle life
        TCurve<UFloat> fAnimCurve;      // Animation curve

        STextureTiling()
        {
            TCurve<UFloat>::source_spline source;
            TCurve<UFloat>::key_type key;
            key.time = 0.0f;
            key.value = 0.0f;
            key.flags = SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_OUT_SHIFT;
            source.insert_key(key);
            key.time = 1.0f;
            key.value = 1.0f;
            key.flags = SPLINE_KEY_TANGENT_LINEAR << SPLINE_KEY_TANGENT_IN_SHIFT;
            source.insert_key(key);
            fAnimCurve.from_source(source);
        }

        uint GetTileCount() const
        {
            return nTilesX * nTilesY - nFirstTile;
        }

        uint GetFrameCount() const
        {
            return nVariantCount * nAnimFramesCount;
        }

        float GetAnimPos(float fAge, float fRelativeAge) const
        {
            // Select anim frame based on particle age.
            float fAnimPos = 0.0f;
            if (fAnimFramerate > 0.f)
            {
                fAnimPos = fAge * fAnimFramerate / nAnimFramesCount;
                if (eAnimCycle == eAnimCycle.Loop)
                {
                    fAnimPos = fmod(fAnimPos, 1.f);
                }
                else if (eAnimCycle == eAnimCycle.Mirror)
                {
                    fAnimPos = 1.f - abs(fmod(fAnimPos, 2.f) - 1.f);
                }
                else
                {
                    fAnimPos = min(fAnimPos, 1.f);
                }
            }
            else
            {
                fAnimPos = fRelativeAge;
            }
            fAnimPos = fAnimCurve(fAnimPos);
            return fAnimPos;
        }

        float GetAnimPosScale() const
        {
            // Scale animation position to frame number
            if (eAnimCycle)
            {
                return float(nAnimFramesCount);
            }
            else if (bAnimBlend)
            {
                return float(nAnimFramesCount - 1);
            }
            else
            {
                // If not cycling, reducing slightly to avoid hitting 1.
                return float(nAnimFramesCount) - 0.001f;
            }
        }

        void Correct()
        {
            nFirstTile = aznumeric_caster(min<uint>(nFirstTile, nTilesX * nTilesY - 1));
            nAnimFramesCount = min<uint>(nAnimFramesCount, GetTileCount());
            nVariantCount = min<uint>(nVariantCount, GetTileCount() / nAnimFramesCount);
        }

        AUTO_STRUCT_INFO
    };

    STextureTiling TextureTiling;                   // Tiling of texture for animation and variation
    STextureTiling GlowTextureTiling;               // Tiling of glow texture
    TSmallBool bTessellation;                           // If hardware supports, tessellate particles for better shadowing and curved connected particles
    TSmallBool bOctagonalShape;                     // Use octagonal shape for textures instead of quad
    struct SSoftParticle
        : TSmallBool
    {
        UFloat fSoftness;                  // Soft particles scale
        SSoftParticle()
            : fSoftness(1.0f) {}
        AUTO_STRUCT_INFO
    } bSoftParticle;                                            // Soft intersection with background
    struct SMotionBlur
        : TSmallBool
    {
        UFloat fBlurStrength; // strength of blur from object motion
        SMotionBlur()
            : fBlurStrength(PARTICLE_PARAMS_DEFAULT_BLUR_STRENGTH) {}
        AUTO_STRUCT_INFO
    } bMotionBlur; //motion blur
    CCryName sGeometry;                                     // Geometry for 3D particles
    DEFINE_ENUM(EGeometryPieces,
        Whole,
        RandomPiece,
        AllPieces
        )
    EGeometryPieces eGeometryPieces;                // Which geometry pieces to emit
    TSmallBool bNoOffset;                                   // Disable centering of geometry
    TVarEPParamRandLerp<UFloat> fAlpha;                     // <SoftMax=1> Alpha value (opacity, or multiplier for additive)
    struct SAlphaClip
    {
        TRangeParam<UFloat>         fScale;                     // <SoftMax=1> Final alpha multiplier, for particle alpha 0 to 1
        TRangeParam<UnitFloat>  fSourceMin;             // Source alpha clip min, for particle alpha 0 to 1
        TRangeParam<UnitFloat>  fSourceWidth;           // Source alpha clip range, for particle alpha 0 to 1

        SAlphaClip()
            : fScale(0, 1)
            , fSourceWidth(1, 1) {}

        AUTO_STRUCT_INFO
    } AlphaClip;                                                    // Alpha clipping settings, for particle alpha 0 to 1
    TVarEPParamRandLerp<Color3F> cColor;                    // Color modulation

    // <Group=Lighting>
    UFloat fDiffuseLighting;                            // <Max=1000> Multiplier for particle dynamic lighting
    UnitFloat fDiffuseBacklighting;             // Fraction of diffuse lighting applied in all directions
    UFloat fEmissiveLighting;                           // <Max=1000> Multiplier for particle emissive lighting. won't larger then sun's max intensity
    UnitFloat fEnvProbeLighting;                        //scalar of how much environment probe lighting applied to particle

    TSmallBool bReceiveShadows;                     // Shadows will cast on these particles
    TSmallBool bCastShadows;                            // Particles will cast shadows (currently only geom particles)
    TSmallBool bNotAffectedByFog;                   // Ignore fog
    TSmallBool bGlobalIllumination;             // Enable global illumination in the shader

    struct SLightSource
    {
        TSmallBool bAffectsThisAreaOnly;        // Affect current clip volume only
        TVarEPParam<UFloat> fRadius;                // <SoftMax=10> Radius of light
        TVarEPParam<UFloat> fIntensity;         // <SoftMax=10> Intensity of light (color from Appearance/Color)
        AUTO_STRUCT_INFO
    } LightSource;                                              // Per-particle light generation

    // <Group=Audio>
    CCryName sStartTrigger;                             // Audio start trigger to execute.
    CCryName sStopTrigger;                              // Audio stop trigger to execute.
    TVarEParam<UFloat> fSoundFXParam;           // Custom real-time sound modulation parameter
    DEFINE_ENUM(ESoundControlTime,
        EmitterLifeTime,
        EmitterExtendedLifeTime,
        EmitterPulsePeriod
        )
    ESoundControlTime eSoundControlTime;        // The sound control time type

    // <Group=Size>
    TVarEPParam<UFloat> fSize;                      // <SoftMax=8> Particle radius, for sprites; size scale for geometry
    TVarEPParam<UFloat> fAspect;                    // <SoftMax=8> X-to-Y scaling factor
    TVarEPParam<UFloat> fSizeX;                     // <SoftMax=800> X Size for sprites; size scale for geometry
    TVarEPParam<UFloat> fSizeY;                     // <SoftMax=800> Y size
    TVarEPParam<UFloat> fSizeZ;                     // <SoftMax=800> Z size; only usful for geometry particle
    TSmallBool bMaintainAspectRatio;                // Maintain aspect ratio in Editor AND Game
    TVarEPParam<SFloat> fPivotX;                    // <SoftMin=-1> <SoftMax=1> Pivot offset in X direction
    TVarEPParam<SFloat> fPivotY;                    // <SoftMin=-1> <SoftMax=1> Pivot offset in Y direction
    TVarEPParam<SFloat> fPivotZ;                    // <SoftMin=-1> <SoftMax=1> Pivot offset in Z directions; only usful for geometry particle

    struct SStretch
        : TVarEPParam<UFloat>
    {
        SFloat fOffsetRatio;                                // <SoftMin=-1> $<SoftMax=1> Move particle center this fraction in direction of stretch
        AUTO_STRUCT_INFO
    } fStretch;                                                     // <SoftMax=1> Stretch particle into moving direction, amount in seconds

    struct STailLength
        : TVarEPParam<UFloat>
    {
        uint8 nTailSteps;                                       // <SoftMax=16> Number of tail segments
        AUTO_STRUCT_INFO
    } fTailLength;                                              // <SoftMax=10> Length of tail in seconds
    UFloat fMinPixels;                                      // <SoftMax=10> Augment true size with this many pixels

    // <Group=ShapeParams>
    TVarEParam<UFloat> fEmitterSizeDiameter;
    Vec3_TVarEParam_SFloat vEmitterSizeXYZ;
    TVarEParam<UFloat> fSpawnOffset;
    TSmallBool bConfineX;                           // Flag to confine per particle age to extents of box size along x axis
    TSmallBool bConfineY;                           // Flag to confine per particle age to extents of box size along y axis
    TSmallBool bConfineZ;                           // Flag to confine per particle age to extents of box size along z axis
    Vec3_TVarEParam_SFloat vSpawnPosXYZ;                        
    Vec3_TVarEParam_UFloat vSpawnPosXYZRandom;                  
    Vec3_TVarEParam_SFloat vSpawnPosIncrementXYZ;
    Vec3_TVarEParam_UFloat vSpawnPosIncrementXYZRandom;
    Vec3_TVarEPParam_SFloat vVelocity; // Simplified speed controller
    Vec3_TVarEParam_UFloat vVelocityXYZRandom;
    TVarEParam<UFloat> fVelocitySpread; //conical distribution of velocity

    //Steven, confetti - use these as input params, the final resulting vars are these + inherited.
    TSmallBool bGroup;
    TSmallBool bLocalInheritedGroupData;                    // used to signal that there is group data to use
    Vec3S vLocalSpawnPosOffset;                             // Spawn offset from the emitter position
    Vec3U vLocalSpawnPosRandomOffset;                       // Spawn offset from the emitter position
    UnitFloat fLocalOffsetRoundness;                        // Fraction of emit volume corners to round: 0 = box, 1 = ellips
    TVarEParam<UnitFloat> fLocalSpawnIncrement;             // Fraction of inner emit volume to avoid (If you're confused by the name, it's because "local spawn increment" only *looks* like English. Ist's actually a little-known ancient language, and when literally translated means "Fraction of inner emit volume to avoid")
    Vec3_tpl<SAngle> vLocalInitAngles;                      // Initial rotation in symmetric angles (degrees)
    Vec3_tpl<UFullAngle> vLocalRandomAngles;                // Bidirectional random angle variation
    SStretch fLocalStretch;                                 // <SoftMax=1> Stretch particle into moving direction, amount in seconds

    //Steven, Confetti - use this struct to store inherited group data
    struct InheritedGroupParams
    {
        InheritedGroupParams () :
            vPositionOffset(ZERO),
            vRandomOffset(ZERO),
            vInitAngles(ZERO),
            vRandomAngles(ZERO)
        { }
        TSmallBool bInheritedGroupData;                     // used to signal that there is group data to use
        Vec3S vPositionOffset;                             // Spawn offset from the emitter position
        Vec3U vRandomOffset;                               // Spawn offset from the emitter position
        UnitFloat fOffsetRoundness;                         // Fraction of emit volume corners to round: 0 = box, 1 = ellipsoid
        TVarEParam<UnitFloat> fOffsetInnerFraction;                     // Fraction of inner emit volume to avoid
        Vec3_tpl<SAngle> vInitAngles;                       // Initial rotation in symmetric angles (degrees)
        Vec3_tpl<UFullAngle> vRandomAngles;                 // Bidirectional random angle variation
        SStretch fStretch;                                  // <SoftMax=1> Stretch particle into moving direction, amount in seconds
        void Clear()
        {
            bInheritedGroupData = false;
            vPositionOffset = Vec3S();
            vRandomOffset = Vec3U();
            fOffsetRoundness = 0.f;
            fOffsetInnerFraction = TVarEParam<UnitFloat>(0.f);
            vInitAngles =  Vec3_tpl<SAngle>();
            vRandomOffset.zero();
            fStretch.Set(0);
        }
    } inheritedGroup;

    // <Group=Trail> - Chris Hekman, Confetti
    TSmallBool bLockAnchorPoints;
    CCryName sTrailFading;


    // Connection
    struct SConnection
        : TSmallBool
    {
        TSmallBool bConnectParticles;                           // Particles are drawn connected serially
        TSmallBool bConnectToOrigin;                            // Newest particle connected to emitter origin
        DEFINE_ENUM(ETextureMapping,
            PerParticle,
            PerStream
            )
        ETextureMapping eTextureMapping;                        // Basis of texture coord mapping (particle or stream)
        TSmallBool bTextureMirror;                              // Mirror alternating texture tiles; else wrap
        UFloat fTextureFrequency;                                   // <SoftMax=8> Number of texture wraps in line

        SConnection()
            : bTextureMirror(true)
            , fTextureFrequency(1.f) {}
        AUTO_STRUCT_INFO
    } Connection;

    TSmallBool bMinVisibleSegmentLength;
    UFloat fMinVisibleSegmentLengthFloat;
    struct SCameraNonFacingFade
        : public TSmallBool
    {
        TCurve<UFloat> fFadeCurve;
        AUTO_STRUCT_INFO
    } bCameraNonFacingFade;

    // <Group=Movement>
    TVarEParam<SFloat> fSpeed;                      // Initial speed of a particle
    SFloat fInheritVelocity;                            // <SoftMin=0> $<SoftMax=1> Fraction of emitter's velocity to inherit
    Vec3S vVelocityAlongAxis;                             // velocity along asix for Point Emitter
    Vec3S vRandomVelocityAlongAxis;                       // Random velocity along asix for Point Emitter

    struct SAirResistance
        : TVarEPParam<UFloat>
    {
        UFloat fRotationalDragScale;                // <SoftMax=1> Multiplier to AirResistance, for rotational motion
        UFloat fWindScale;                                  // <SoftMax=1> Artificial adjustment to environmental wind

        SAirResistance()
            : fRotationalDragScale(1)
            , fWindScale(1) {}
        AUTO_STRUCT_INFO
    }   fAirResistance;                                         // <SoftMax=10> Air drag value, in inverse seconds
    TVarEPParam<SFloat> fGravityScale;      // <SoftMin=-1> $<SoftMax=1> Multiplier for world gravity
    Vec3S vAcceleration;                                    // Explicit world-space acceleration vector
    TVarEPParam<UFloat> fTurbulence3DSpeed;// <SoftMax=10> 3D random turbulence force
    TVarEPParam<UFloat> fTurbulenceSize;    // <SoftMax=10> Radius of vortex rotation (axis is direction of movement)
    TVarEPParam<SFloat> fTurbulenceSpeed;   // <SoftMin=-360> $<SoftMax=360> Angular speed of vortex rotation

    DEFINE_ENUM(EMoveRelative,
        No,
        Yes,
        YesWithTail
        )
    EMoveRelative eMoveRelEmitter;                  // Particle motion is in emitter space
    TSmallBool bBindEmitterToCamera;            // Emitter attached to main render camera
    TSmallBool bSpaceLoop;                              // Loops particles within emission volume, or within Camera Max Distance

    struct STargetAttraction
    {
        DEFINE_ENUM(ETargeting,
            External,
            OwnEmitter,
            Ignore
            )
        ETargeting eTarget;                                     // Source of target attractor
        TSmallBool bExtendSpeed;                        // Extend particle speed as necessary to reach target in normal lifetime
        TSmallBool bShrink;                                 // Shrink particle as it approaches target
        TSmallBool bOrbit;                                  // Orbit target at specified distance, rather than disappearing
        TVarEPParam<SFloat> fRadius;                // Radius of attractor, for vanishing or orbiting
        AUTO_STRUCT_INFO
    } TargetAttraction;                                     // Specify target attractor behavior

    // <Group=Rotation>
    Vec3_tpl<SAngle> vInitAngles;           // Initial rotation in symmetric angles (degrees)
    Vec3_tpl<UFullAngle> vRandomAngles;     // Bidirectional random angle variation
    TVarEPParam<SFloat> vRotationRateX;     // <SoftMin=-360> $<SoftMax=360> Rotation speed (degree/sec)
    TVarEPParam<SFloat> vRotationRateY;     // <SoftMin=-360> $<SoftMax=360> Rotation speed (degree/sec)
    TVarEPParam<SFloat> vRotationRateZ;     // <SoftMin=-360> $<SoftMax=360> Rotation speed (degree/sec)

    // <Group=Collision>
    DEFINE_ENUM(EDepthCollision,
        None,
        FrameBuffer,
        Cubemap)
    EDepthCollision eDepthCollision;   // for GPU particles, how should they collide
    UFloat fCubemapFarDistance; // For cubemap depth collision, what is the far plane of the cubemap
#if PARTICLES_USE_CRY_PHYSICS
    DEFINE_ENUM_VALS(EPhysics, uint8,
        None = 0,
        SimpleCollision = 1,
        SimplePhysics = 2,
        RigidBody = 3
    )
#else
    DEFINE_ENUM_VALS(EPhysics, uint8,
        None = 0,
        SimpleCollision = 1,
        RigidBody = 3
    )
#endif
    EPhysics ePhysicsType;                                  // What kind of physics simulation to run on particle
// AZPhysics
    CAZPhysicsCollisionLayerIndex sCollisionLayer;             // Collision layer to use
    CAZPhysicsCollisionGroupIndex sCollisionGroup;               // Collision group to collide with
// ---------
    TSmallBool bCollideTerrain;                     // Collides with terrain (if Physics <> none)
    TSmallBool bCollideStaticObjects;           // Collides with static physics objects (if Physics <> none)
    TSmallBool bCollideDynamicObjects;      // Collides with dynamic physics objects (if Physics <> none)
    UnitFloat8 fCollisionFraction;              // Fraction of emitted particles that actually perform collisions
    UFloat fCollisionCutoffDistance;            // Maximum distance up until collisions are respected (0 = infinite)
    uint8 nMaxCollisionEvents;                      // Max # collision events per particle (0 = no limit)
    DEFINE_ENUM(ECollisionResponse,
        Die,
        Bounce,
        Stop
        )
    ECollisionResponse eFinalCollision;         // What to do on final collision (when MaxCollisions > 0)
// AZPhysics
    CSurfaceTypeIndex sSurfaceType;             // Surface type for physicalized particles
// CryPhysics
    TSmallBool bUseSelfCollision;                  // Whether to ignore collision behavior (die/stop) with particles of this same emitter
    CAZPhysicsMaterialIndex sPhysicsMaterial;           // Physics material to use for particles body (from Default material library only)
// ----------
    SFloat fElasticity;                                     // <SoftMin=0> $<SoftMax=1> Collision bounce coefficient: 0 = no bounce, 1 = full bounce
    UFloat fDynamicFriction;                            // <SoftMax=10> Sliding drag value, in inverse seconds
    UFloat fThickness;                                      // <SoftMax=1> Lying thickness ratio - for physicalized particles only
    UFloat fDensity;                                            // <SoftMax=2000> Mass density for physicslized particles

    // <Group=Visibility>
    UFloat fViewDistanceAdjust;                     // <SoftMax=1> Multiplier to automatic distance fade-out
    UFloat fCameraMaxDistance;                      // <SoftMax=100> Max distance from camera to render particles
    UFloat fCameraMinDistance;                      // <SoftMax=100> Min distance from camera to render particles
    SFloat fCameraDistanceOffset;                   // <SoftMin=-1> <SoftMax=1> Offset the emitter away from the camera
    UFloat fCameraFadeNearStrength;                 // Strength of the camera distance fade at the near end (only with GPU particles)
    UFloat fCameraFadeFarStrength;                  // Strength of the camera distance fade at the far end
    SFloat fSortOffset;                                     // <SoftMin=-1> <SoftMax=1> Offset distance used for sorting
    SFloat fSortBoundsScale;                            // <SoftMin=-1> <SoftMax=1> Choose emitter point for sorting; 1 = bounds nearest, 0 = origin, -1 = bounds farthest
    TSmallBool bDynamicCulling;                     // Force enable Dynamic Culling. This disables culling of particle simulation to get accurate bounds for render culling.
    TSmallBool bDrawNear;                                   // Render particle in near space (weapon)
    TSmallBool bDrawOnTop;                              // Render particle on top of everything (no depth test)
    ETrinary tVisibleIndoors;                           // Whether visible indoors / outdoors / both
    ETrinary tVisibleUnderwater;                    // Whether visible under / above water / both

    // <Group=Advanced>
    DEFINE_ENUM(EForce,
        None,
        Wind,
        Gravity,
        // Compatibility
        _Target
        )
    EForce eForceGeneration;                                // Generate physical forces if set
    UFloat fFillRateCost;                                   // Adjustment to max screen fill allowed per emitter
#ifdef PARTICLE_MOTION_BLUR
    UFloat fMotionBlurScale;                            // Multiplier for motion blur texture blurring
    UFloat fMotionBlurStretchScale;             // Multiplier for motion blur sprite stretching based on particle movement
    UFloat fMotionBlurCamStretchScale;      // Multiplier for motion blur sprite stretching based on camera movement
#endif
    TFixed<uint8, MAX_HEATSCALE> fHeatScale;  // Multiplier to thermal vision
    UnitFloat fSphericalApproximation;      // Align the particle to the tangent of the sphere
    UFloat fPlaneAlignBlendDistance;      // Distance when blend to camera plane aligned particles starts

    TRangedType<uint8, 0, 2> nSortQuality;    // Sort new particles as accurately as possible into list, by main camera distance
    TSmallBool bHalfRes;                                    // Use half resolution rendering
    TSmallBoolTrue bStreamable;                         // Texture/geometry allowed to be streamed
    TSmallBool bVolumeFog;                              // Use as a participating media of volumetric fog
    Unit4Float fVolumeThickness;                        // Thickness factor for particle size
    TSmallBoolTrue DepthOfFieldBlur;                    // Particles won't be affected by depthOfField post effect if set to false
    TSmallBool FogVolumeShadingQualityHigh;             // Particle vertex shading quality: - 0: Standard - 1: fog volumes are handled more accurately
    uint8 nParticleSizeDiscard;

    // <Group=Configuration>
    DEFINE_ENUM(EConfigSpecBrief,
        Low,
        Medium,
        High,
        VeryHigh
        )
    EConfigSpecBrief eConfigMin;                        // Minimum config spec this effect runs in
    EConfigSpecBrief eConfigMax;                    // Maximum config spec this effect runs in

    //Confetti - Begin
    //Beam Parameters
    Vec3S vTargetPosition;                          // location the beam should end at
    Vec3U vTargetRandOffset;                        // random offset from the target position the beam will end at
    Vec3S vBeamUpVector;
    TVarEParam<UFloat> fBeamAge; //how long the beam should stay alive
    //Beam Segment Parameters
    UFloat fSegmentCount;                           // number of segments per beam
    UFloat fSegmentLength;                          // length of segments per beam
    UFloat fTextureShift;
    DEFINE_ENUM(ESegmentType,                       // type of quad segments generated per beam
        FIXED,                                      // beam will have this number of segments with the length calculated evenly
        LENGTH                                      // beam will have n number of segments each this length long
        )
    ESegmentType eSegmentType;
    DEFINE_ENUM(EBeamWaveType,                      // type of wave used to generate a beam emitter
        NONE,
        SINE,
        SQUARE,
        NOISE
        )
    EBeamWaveType eBeamWaveType;
    DEFINE_ENUM(EBeamWaveTangentSource,             // We cannot use amplitude, frequency, start position, off position, start tangent, and end tangent as constants. we allow the user to select either the start tangent or the end tangent instead.
        ORIGIN,
        TARGET
        )
    EBeamWaveTangentSource eBeamWaveTangentSource;
    TVarEParam<UFloat> fTangent;
    UFloat fAmplitude;
    UFloat fFrequency;
    //Confetti - End

    struct SPlatforms
    {
        TSmallBoolTrue  PCDX11, Provo, Xenia, Salem, hasIOS,
                        hasAndroid, hasMacOSGL, hasMacOSMetal;
        AUTO_STRUCT_INFO
    } Platforms;                                                    // Platforms this effect runs on

    ParticleParams()
        : bEnabled(true)
        , fSize(1.f)
        , fAspect(1.f)
        , fSizeX(1.f)
        , fSizeY(1.f)
        , fSizeZ(1.f)
        , bMaintainAspectRatio(true)
        , cColor(1.f)
        , fAlpha(1.f)
        , eBlendType(EBlend::AlphaBased)
        , fDiffuseLighting(1.f)
        , fCurvature(1.f)
        , fViewDistanceAdjust(1.f)
        , fCameraFadeNearStrength(1.f)
        , fCameraFadeFarStrength(1.f)
        , fFillRateCost(1.f)
        , fDensity(1000.f)
        , fThickness(1.f)
        , fCollisionFraction(1.f)
        , fElasticity(1.f)
#ifdef PARTICLE_MOTION_BLUR
        , fMotionBlurScale(1.f)
        , fMotionBlurStretchScale(1.f)
        , fMotionBlurCamStretchScale(1.f)
#endif
        , fSphericalApproximation(1.f)
        , fVolumeThickness(1.0f)
        , fSoundFXParam(1.f)
        , fSortConvergancePerFrame(1.f)
        , eConfigMax(EConfigSpecBrief::VeryHigh)
        // the default vec3 constructor sets values to nan if _DEBUG is defined.  This occurs after the ZeroInit base class initialization.  Reset these to zero.
        , vLocalInitAngles(ZERO)
        , vLocalRandomAngles(ZERO)
        , vAcceleration(ZERO)
        , vInitAngles(ZERO)
        , vRandomAngles(ZERO)
        , vSpawnPosOffset(ZERO)
        , vSpawnPosRandomOffset(ZERO)
        , vLocalSpawnPosOffset(ZERO)
        , vLocalSpawnPosRandomOffset(ZERO)
        , vBeamUpVector(ZERO)
        , vTargetPosition(Vec3(0, 0, 15.0f))
        , vVelocityAlongAxis(ZERO)
        , vRandomVelocityAlongAxis(ZERO)
        , vSpawnPosXYZ(ZERO)
        , vSpawnPosXYZRandom(ZERO)
        , vSpawnPosIncrementXYZ(ZERO)
        , vSpawnPosIncrementXYZRandom(ZERO)
        , vVelocityXYZRandom(ZERO)
        // these are default values to allow beam emitters to show at the start until better defaults are provided
        , vTargetRandOffset(Vec3(3.0f, 3.0f, 0.0f))
        , fSegmentCount(10.0f)
        , fBeamAge(2.0f)
        , bCameraNonFacingFade(SCameraNonFacingFade())
        , fCubemapFarDistance(20.0f)
        , nParticleSizeDiscard(0)
        , fVelocitySpread(360.f)
#if !PARTICLES_USE_CRY_PHYSICS
        , bUseSelfCollision(false)
#endif
    {}

    ParticleParams(type_zero)
        // the default vec3 constructor sets values to nan if _DEBUG is defined.  This occurs after the ZeroInit base class initialization.  Reset these to zero.
        : vLocalInitAngles(ZERO)
        , vLocalRandomAngles(ZERO)
        , vAcceleration(ZERO)
        , vInitAngles(ZERO)
        , vRandomAngles(ZERO)
        , vTargetPosition(ZERO)
        , vTargetRandOffset(ZERO)
        , vSpawnPosOffset(ZERO)
        , vSpawnPosRandomOffset(ZERO)
        , vLocalSpawnPosOffset(ZERO)
        , vLocalSpawnPosRandomOffset(ZERO)
        , vBeamUpVector(ZERO)
        , vVelocityAlongAxis(ZERO)
        , vRandomVelocityAlongAxis(ZERO)
        , vSpawnPosXYZ(ZERO)
        , vSpawnPosXYZRandom(ZERO)
        , vSpawnPosIncrementXYZ(ZERO)
        , vSpawnPosIncrementXYZRandom(ZERO)
        , vVelocityXYZRandom(ZERO)
    {}

    // Derived properties
    bool IsImmortal() const
    {
        return bEnabled && ((bContinuous && !fEmitterLifeTime && fCount) || fPulsePeriod || !fParticleLifeTime);
    }
    bool HasEquilibrium() const
    {
        // Effect reaches a steady state: No emitter lifespan or pulsing.
        return bContinuous && fCount && fParticleLifeTime && !fPulsePeriod && !fEmitterLifeTime;
    }
    float GetMaxSpawnDelay() const
    {
        return eSpawnIndirection >= ESpawn::ParentCollide ? fHUGE : fSpawnDelay.GetMaxValue();
    }
    float GetMaxEmitterLife() const
    {
        return GetMaxSpawnDelay() + (bContinuous ? ZeroIsHuge(fEmitterLifeTime.GetMaxValue()) : 0.f);
    }
    float GetMaxParticleLife() const
    {
        return if_pos_else(fParticleLifeTime.GetMaxValue(), fParticleLifeTime.GetMaxValue(), ZeroIsHuge(fEmitterLifeTime.GetMaxValue()));
    }
    uint8 GetTailSteps() const
    {
        return fTailLength ? fTailLength.nTailSteps : 0;
    }
    bool HasVariableVertexCount() const
    {
        return GetTailSteps() || IsConnectedParticles();
    }
    bool IsConnectedParticles() const
    {
        return GetEmitterShape() == EEmitterShapeType::TRAIL || GetEmitterShape() == EEmitterShapeType::BEAM || Connection;
    }
    float GetMaxRotationAngle() const
    {
        bool hasRotX = (vRotationRateX.IsConstant() && vRotationRateX.GetMinValue() != 0.0f);
        bool hasRotZ = (vRotationRateZ.IsConstant() && vRotationRateZ.GetMinValue() != 0.0f);
        if (hasRotX || hasRotZ || vRotationRateX.GetRandomRange() || vRotationRateZ.GetRandomRange())
        {
            return gf_PI;
        }
        return DEG2RAD(max(abs(vInitAngles.x) + abs(vRandomAngles.x), abs(vInitAngles.z) + abs(vRandomAngles.z)));
    }
    AABB GetEmitOffsetBounds() const
    {
        const float percentageToScale = 0.01f;

        const float fEmissionStrength = fEmitterLifeTime.GetMaxValue();

        Vec3 spawnPosMax = vSpawnPosXYZ.GetMaxVector();
        Vec3 spawnPosMin = vSpawnPosXYZ.GetMinVector();
        Vec3 spawnPosRand = vSpawnPosXYZRandom.GetMaxVector();
        float spawnOffsetMax = fSpawnOffset.GetMaxValue();
        float emitterSizeMax = fEmitterSizeDiameter.GetMaxValue();

        Vec3 emitterSizeXYZMax = vEmitterSizeXYZ.GetMaxVector() * PARTICLE_PARAMS_WIDTH_TO_HALF_WIDTH;

        Vec3 maxIncrement = vSpawnPosIncrementXYZ.GetMaxVector();
        Vec3 minIncrement = vSpawnPosIncrementXYZ.GetMinVector();
        Vec3 incrementRand = vSpawnPosIncrementXYZRandom.GetMaxVector();

        Vec3 spawnMin = vSpawnPosOffset - vSpawnPosRandomOffset * PARTICLE_PARAMS_WIDTH_TO_HALF_WIDTH;
        Vec3 spawnMax = vSpawnPosOffset + vSpawnPosRandomOffset * PARTICLE_PARAMS_WIDTH_TO_HALF_WIDTH;

        switch (GetEmitterShape())
        {
            case ParticleParams::EEmitterShapeType::CIRCLE:
            {
                float x = spawnPosMax.x + spawnPosRand.x + emitterSizeMax * PARTICLE_PARAMS_WIDTH_TO_HALF_WIDTH + emitterSizeMax * ceilf(maxIncrement.x) * percentageToScale;
                float maxZ = spawnPosMax.z + spawnPosRand.z + emitterSizeMax * maxIncrement.z * percentageToScale;
                float minZ = spawnPosMin.z - spawnPosRand.z - emitterSizeMax * minIncrement.z * percentageToScale;

                spawnMax += Vec3(x, x, maxZ);
                spawnMin += Vec3(-x, -x, minZ);

                break;
            }
            case ParticleParams::EEmitterShapeType::SPHERE:
            {
                float x = spawnPosMax.x + spawnPosRand.x + emitterSizeMax * PARTICLE_PARAMS_WIDTH_TO_HALF_WIDTH + emitterSizeMax * maxIncrement.x * percentageToScale;

                spawnMax += Vec3(x, x, x);
                spawnMin += Vec3(-x, -x, -x);
  
                break;
            }
            case ParticleParams::EEmitterShapeType::POINT:
            {
                float maxX = spawnPosMax.x + spawnPosRand.x + spawnOffsetMax;
                float minX = spawnPosMin.x - spawnPosRand.x - spawnOffsetMax;
                float maxY = spawnPosMax.y + spawnPosRand.y + spawnOffsetMax;
                float minY = spawnPosMin.y - spawnPosRand.y - spawnOffsetMax;
                float maxZ = spawnPosMax.z + spawnPosRand.z + spawnOffsetMax;
                float minZ = spawnPosMin.z - spawnPosRand.z - spawnOffsetMax;

                spawnMax += Vec3(maxX, maxY, maxZ);
                spawnMin += Vec3(minX, minY, minZ);

                break;
            }
            case ParticleParams::EEmitterShapeType::BOX:
            {
                Vec3 min = spawnPosMin;
                Vec3 max = spawnPosMax;

                min.CheckMin(spawnPosMax);
                max.CheckMax(spawnPosMin);

                //bbox for spawn position
                spawnMax = max + spawnMax + spawnPosRand; //max spawn position + max offset + spawn position random
                spawnMin =  min + spawnMin - spawnPosRand; //min spawn position + min offset - spawn position random

                //shift to emitter box's corner (positive X, negative Y and Z). Check Particle::InitPos function for detail
                Vec3 cornerOffset = Vec3(emitterSizeXYZMax.x, -emitterSizeXYZMax.y, -emitterSizeXYZMax.z);
                spawnMax += cornerOffset;
                spawnMin += cornerOffset;
                break;
            }
            default:
            {
                break;
            }
        }
        return AABB(spawnMin, spawnMax);
    }
    float GetFullTextureArea() const
    {
        // World area corresponding to full texture, thus multiplied by tile count.
        return sqr(fSizeX.GetMaxValue() * fSizeY.GetMaxValue()) * float(TextureTiling.nTilesX * TextureTiling.nTilesY);
    }
    float GetAlphaFromMod(float fMod, float lerp) const
    {
        return fAlpha.GetVarValue(lerp) * AlphaClip.fScale.Interp(fMod);
    }
    Vec3 GetVelocityVector(float e, float p) const
    {
        //don't apply random, it is applied pre-emission.
        Vec3 result(vVelocity.GetVector(0.f, e, p));
        return result;
    }
    Vec3 GetSpawnPos() const
    {
        return vSpawnPosOffset;
    }
    Vec3 GetEmitterOffset(Vec3 vEmitBox, Vec3 vEmitScale, float fEmissionStrength, Vec3 emitterPos, Vec3 randomOffset) const
    {
        const UnitFloat innerFraction = fSpawnIncrement(VRANDOM, fEmissionStrength);

        Vec3 emitBox = vEmitBox;

        // Loop until uniform offset is found
        Vec3 spawnPosOffset(0, 0, 0);
        for (;;)
        {
            spawnPosOffset = Vec3(cry_random(-1.0f, 1.0f) * randomOffset.x,
                cry_random(-1.0f, 1.0f) * randomOffset.y,
                cry_random(-1.0f, 1.0f) * randomOffset.z);

            if (fOffsetRoundness)
            {
                // Loop until random point is within rounded volume.
                Vec3 vR(max(abs(spawnPosOffset.x) - emitBox.x, 0.f), max(abs(spawnPosOffset.y) - emitBox.y, 0.f), max(abs(spawnPosOffset.z) - emitBox.z, 0.f));
                float vRSqr = sqr(vR.x * vEmitScale.x) + sqr(vR.y * vEmitScale.y) + sqr(vR.z * vEmitScale.z);
                if (vRSqr > 1.f)
                {
                    continue;
                }
            }

            // Apply inner volume subtraction.
            if (innerFraction)
            {
                // Find max possible scaled offset.
                float fScaleMax = 
                    div_min(
                        randomOffset.x, 
                        abs(spawnPosOffset.x), 
                        div_min(
                            randomOffset.y, 
                            abs(spawnPosOffset.y), 
                            div_min(randomOffset.z, 
                                abs(spawnPosOffset.z), 
                                fHUGE)));
                Vec3 vOffsetMax = spawnPosOffset * fScaleMax;
                if (fOffsetRoundness)
                {
                    Vec3 vRMax(max(abs(vOffsetMax.x) - emitBox.x, 0.f), max(abs(vOffsetMax.y) - emitBox.y, 0.f), max(abs(vOffsetMax.z) - emitBox.z, 0.f));
                    float fRMaxSqr = sqr(vRMax.x * vEmitScale.x) + sqr(vRMax.y * vEmitScale.y) + sqr(vRMax.z * vEmitScale.z);
                    if (fRMaxSqr > 0.f)
                    {
                        vRMax *= isqrt_tpl(fRMaxSqr);
                        for (int a = 0; a < 3; a++)
                        {
                            if (vRMax[a] > 0.f)
                            {
                                vOffsetMax[a] = (emitBox[a] + vRMax[a]) * fsgnf(vOffsetMax[a]);
                            }
                        }
                    }
                }

                // Interpolate between current ans max offsets.
                spawnPosOffset += (vOffsetMax - spawnPosOffset) * innerFraction;
            }

            break;
        }

        emitterPos += spawnPosOffset;

        return emitterPos;
    }

    Vec3i ConfinedParticle() const
    {
        if (GetEmitterShape() == EEmitterShapeType::BOX)
        {
            return Vec3i(bConfineX ? 1 : 0, bConfineY ? 1 : 0, bConfineZ ? 1 : 0);
        }
        return Vec3i(0,0,0);
    }
    inline TVarEParam<UFloat> GetCount() const
    {
        if (GetEmitterShape() == EEmitterShapeType::BEAM)
        {
            return fBeamCount;
        }
        return fCount;
    }
    inline EEmitterShapeType GetEmitterShape() const
    {
        if (eEmitterType == EEmitterType::CPU)
        {
            return eEmitterShape;
        }
        else
        {
            switch (eEmitterGpuShape)
            {
            case EEmitterGPUShapeType::ANGLE:
                return EEmitterShapeType::ANGLE;
            case EEmitterGPUShapeType::POINT:
                return EEmitterShapeType::POINT;
            case EEmitterGPUShapeType::CIRCLE:
                return EEmitterShapeType::CIRCLE;
            case EEmitterGPUShapeType::SPHERE:
                return EEmitterShapeType::SPHERE;
            case EEmitterGPUShapeType::BOX:
                return EEmitterShapeType::BOX;
            default:
                return EEmitterShapeType::ANGLE; //just for sanity
                break;
            }
        }
    }

    float GetTexcoordVMultiplier() const
    {
        if (GetEmitterShape() == EEmitterShapeType::TRAIL)
        {
            if (Connection.eTextureMapping == SConnection::ETextureMapping::PerParticle)
            {
                return (fCount * Connection.fTextureFrequency + 1) * 2;
            }
            else
            {
                return (Connection.fTextureFrequency + 1) * 2;
            }
        }
        else if (GetEmitterShape() == EEmitterShapeType::BEAM)
        {
            if (Connection.eTextureMapping == SConnection::ETextureMapping::PerParticle)
            {
                return (fSegmentCount + 1) * 2;
            }
            else
            {
                return 2.0f;
            }
        }
        return 1.0f;
    }

    bool UseTextureMirror() const
    {
        return (GetEmitterShape() == EEmitterShapeType::TRAIL && Connection.bTextureMirror);
    }
    AUTO_STRUCT_INFO
};

#endif // CRYINCLUDE_CRYCOMMON_PARTICLEPARAMS_H
