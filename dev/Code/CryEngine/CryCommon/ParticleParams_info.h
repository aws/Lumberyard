/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYCOMMON_PARTICLEPARAMS_INFO_H
#define CRYINCLUDE_CRYCOMMON_PARTICLEPARAMS_INFO_H
#pragma once

#include "ParticleParams.h"

#if 0
// Search/replace pattern to convert struct member declarations to VAR_INFO declarations
{
    : b*
} {
    .*
} : b + {
    [a - z0 - 9_] +
};
: b*                            // {.*}
\ 1VAR_INFO_ATTRS (\ 3, "\4")

// Version for comment-only lines
// {.*}
ATTRS_INFO("\1")
#endif

STRUCT_INFO_BEGIN(RandomColor)
BASE_INFO(UnitFloat8)
VAR_INFO(m_bRandomHue)
STRUCT_INFO_END(RandomColor)

STRUCT_INFO_T_BEGIN(TVarParam, class, S)
BASE_INFO(S)
VAR_INFO_ATTRS(m_Random, "Random variation, fraction range = [1-Random,1].")
STRUCT_INFO_T_END(TVarParam, class, S)

STRUCT_INFO_T_BEGIN(TVarEParam, class, S)
BASE_INFO(TVarParam<S> )
VAR_INFO_ATTRS(m_EmitterStrength, "Value variance with emitter Strength, or emitter age if Strength < 0")
STRUCT_INFO_T_END(TVarEParam, class, S)

STRUCT_INFO_T_BEGIN(TVarEPParam, class, S)
BASE_INFO(TVarEParam<S> )
VAR_INFO_ATTRS(m_ParticleAge, "Value variance with particle age")
STRUCT_INFO_T_END(TVarEPParam, class, S)

STRUCT_INFO_T_BEGIN(TVarEPParamRandLerp, class, S)
BASE_INFO(TVarEPParam<S> )
VAR_INFO(m_bRandomLerpColor)
VAR_INFO_ATTRS(m_Color, "Other color used for random lerp")
VAR_INFO(m_bRandomLerpStrength)
VAR_INFO_ATTRS(m_EmitterStrength2, "Value variance with emitter Strength, or emitter age if Strength < 0")
VAR_INFO(m_bRandomLerpAge)
VAR_INFO_ATTRS(m_ParticleAge2, "Value variance with particle age")
STRUCT_INFO_T_END(TVarEPParamRandLerp, class, S)

STRUCT_INFO_T_BEGIN(TRangeParam, class, S)
VAR_INFO(Min)
VAR_INFO(Max)
STRUCT_INFO_T_END(TRangeParam, class, S)

STRUCT_INFO_T_BEGIN(Vec3_TVarEParam, class, T)
VAR_INFO_ATTRS(fX, "X Value of the Vector")
VAR_INFO_ATTRS(fY, "Y Value of the Vector")
VAR_INFO_ATTRS(fZ, "Z Value of the Vector")
STRUCT_INFO_T_END(Vec3_TVarEParam, class, T)

STRUCT_INFO_T_BEGIN(Vec3_TVarEPParam, class, T)
VAR_INFO_ATTRS(fX, "X Value of the Vector")
VAR_INFO_ATTRS(fY, "Y Value of the Vector")
VAR_INFO_ATTRS(fZ, "Z Value of the Vector")
STRUCT_INFO_T_END(Vec3_TVarEPParam, class, T)

STRUCT_INFO_BEGIN(ParticleParams::STextureTiling)
VAR_INFO_ATTRS(nTilesX, "Number of columns texture is split into")
VAR_INFO_ATTRS(nTilesY, "Number of rows texture is split into")
VAR_INFO_ATTRS(nFirstTile, "First (or only) tile to use")
VAR_INFO_ATTRS(nVariantCount, "Number of randomly selectable tiles; 0 or 1 if no variation")
VAR_INFO_ATTRS(nAnimFramesCount, "Number of tiles (frames) of animation; 0 or 1 if no animation")
VAR_INFO_ATTRS(fAnimFramerate, "<SoftMax=60> Tex framerate; 0 = 1 cycle / particle life")
VAR_INFO_ATTRS(eAnimCycle, "How animation cycles")
VAR_INFO_ATTRS(bAnimBlend, "Blend textures between frames")
VAR_INFO_ATTRS(fHorizontalFlipChance, "Chance each particle will flip along U axis")
VAR_INFO_ATTRS(fVerticalFlipChance, "Chance each particle will flip along V axis")
VAR_INFO_ATTRS(fAnimCurve, "Animation curve")
STRUCT_INFO_END(ParticleParams::STextureTiling)

ENUM_INFO_BEGIN(EGeomType)
ENUM_ELEM_INFO(, GeomType_None)
ENUM_ELEM_INFO(, GeomType_BoundingBox)
ENUM_ELEM_INFO(, GeomType_Physics)
ENUM_ELEM_INFO(, GeomType_Render)
ENUM_INFO_END(EGeomType)

ENUM_INFO_BEGIN(EGeomForm)
ENUM_ELEM_INFO(, GeomForm_Vertices)
ENUM_ELEM_INFO(, GeomForm_Edges)
ENUM_ELEM_INFO(, GeomForm_Surface)
ENUM_ELEM_INFO(, GeomForm_Volume)
ENUM_INFO_END(EGeomForm)

STRUCT_INFO_BEGIN(ParticleParams)
ATTRS_INFO("<Group=Emitter>")
VAR_INFO(sComment)
VAR_INFO_ATTRS(bEnabled, "Set false to disable this effect")
VAR_INFO_ATTRS(eEmitterType, "Emitter type specifies whether particle simulation runs on the CPU or GPU")
VAR_INFO_ATTRS(eEmitterShape, "Emitter shape specifies the shape of the emitter.")
VAR_INFO_ATTRS(eEmitterGpuShape, "Emitter shape specifies the shape of the emitter.")
VAR_INFO_ATTRS(eInheritance, "Source of ParticleParams used as base for this effect (for serialization, display, etc)")
VAR_INFO_ATTRS(eSpawnIndirection, "Direct: spawn from emitter location; else spawn from each particle in parent emitter")
VAR_INFO_ATTRS(eGPUSpawnIndirection, "Direct: spawn from emitter location, else spawn for each dead particle")
//Note: we have different maximum particle count for cpu and gpu. Search PARTICLE_PARAMS_MAX_COUNT_GPU for detail. 
VAR_INFO_ATTRS(fCount, "<Min=0><Max=" STRINGIFY(PARTICLE_PARAMS_MAX_COUNT_CPU) ">Number of particles alive at once")
VAR_INFO_ATTRS(fBeamCount, "<Max=100>Number of beams alive at once") //Max is arbitrary max reasonable value for this emitter type
VAR_INFO_ATTRS(fMaintainDensity, "<Min = 0><Max = " STRINGIFY(PARTICLE_PARAMS_MAX_MAINTAIN_DENSITY) "> Increase count when emitter moves to maintain spatial density")
VAR_INFO_ATTRS(vVelocity, "Simplified speed controller")

//For level of details - Vera, Confetti
ATTRS_INFO("<Group=LevelOfDetail>")
VAR_INFO_ATTRS(fBlendIn, "Level of detail blend in")
VAR_INFO_ATTRS(fBlendOut, "Level of detail blend out")
VAR_INFO_ATTRS(fOverlap, "Level of detail overlap")

ATTRS_INFO("<Group=Trail>")
VAR_INFO_ATTRS(bLockAnchorPoints, "Locks the vertex anchor points")
VAR_INFO_ATTRS(sTrailFading, "Texture used for faded trail particles")
VAR_INFO_ATTRS(Connection, "Particles are drawn connected serially")

ATTRS_INFO("<Group=ShapeParams>")
VAR_INFO_ATTRS(fEmitterSizeDiameter, "Diameter for Circle and sphere emitter")
VAR_INFO_ATTRS(vEmitterSizeXYZ, "Size in cm")
VAR_INFO_ATTRS(vSpawnPosXYZ, "Spawn position relative to axial coordinates")
VAR_INFO_ATTRS(vSpawnPosXYZRandom, "randomizes the emission of this axial coordinate")
VAR_INFO_ATTRS(fSpawnOffset, "Offset the per particle spawn location in the emitter along its spherical vector for Point emitter")
VAR_INFO_ATTRS(vSpawnPosIncrementXYZ, "% value based on emitter size")
VAR_INFO_ATTRS(vSpawnPosIncrementXYZRandom, "% randomness value")
VAR_INFO_ATTRS(vVelocityXYZRandom, "Random Velocity of the particle emission along axis")
VAR_INFO_ATTRS(fVelocitySpread, "<Min=0><Max=360>conical distribution value in degrees(0-360) when velocity is given")
VAR_INFO_ATTRS(bConfineX, "Confine the particles to a volume matching the emitters shape")
VAR_INFO_ATTRS(bConfineY, "Confine the particles to a volume matching the emitters shape")
VAR_INFO_ATTRS(bConfineZ, "Confine the particles to a volume matching the emitters shape")

//For group nodes - Steven, Confetti
ATTRS_INFO("<Group=GroupNode>")
VAR_INFO_ATTRS(bGroup, "Spawn offset from the emitter position")
VAR_INFO_ATTRS(vLocalSpawnPosOffset, "Spawn offset from the emitter position")
VAR_INFO_ATTRS(vLocalSpawnPosRandomOffset, "Random offset of emission relative position to the spawn position")
VAR_INFO_ATTRS(fLocalOffsetRoundness, "Fraction of emit volume corners to round: 0 = box, 1 = ellipsoid")
VAR_INFO_ATTRS(fLocalSpawnIncrement, "Fraction of inner emit volume to avoid")
VAR_INFO_ATTRS(vLocalInitAngles, "Initial rotation in symmetric angles (degrees)")
VAR_INFO_ATTRS(vLocalRandomAngles, "Bidirectional random angle variation")
VAR_INFO_ATTRS(fLocalStretch, "<SoftMax=1> Stretch particle into moving direction, amount in seconds")

ATTRS_INFO("<Group=Beam>")
VAR_INFO_ATTRS(fBeamAge, "how long should beams stay alive")
VAR_INFO_ATTRS(vTargetPosition, "location the beam should end at")
VAR_INFO_ATTRS(vTargetRandOffset, "random offset from the target position the beam will end at")
VAR_INFO_ATTRS(vBeamUpVector, "determines the number of the peaks and valleys for the wave")
VAR_INFO_ATTRS(fSegmentCount, "<Max=100>number of segments per beam") // Max is arbitrary max reasonable value for this emitter type
VAR_INFO_ATTRS(fSegmentLength, "length of segments per beam")
VAR_INFO_ATTRS(eSegmentType, "type of quad segments generated per beam")
VAR_INFO_ATTRS(eBeamWaveTangentSource, "which source to set the tangent for")
VAR_INFO_ATTRS(fTangent, "The Tangent used for beam emitters")
VAR_INFO_ATTRS(eBeamWaveType, "Type of wave to rander with beam emitters")
VAR_INFO_ATTRS(fAmplitude, "determines the height of the peaks and valleys for the wave")
VAR_INFO_ATTRS(fFrequency, "1.0f/period used for waves ")
VAR_INFO_ATTRS(fTextureShift, "Increments the U coordinate by this value over the particle age")
//End confetti

ATTRS_INFO("<Group=Timing>")
VAR_INFO_ATTRS(bContinuous, "Emit particles gradually until Count reached (rate = Count / ParticleLifeTime)")
VAR_INFO_ATTRS(fSpawnDelay, "Delay the emitter start time by this value	")
VAR_INFO_ATTRS(fEmitterLifeTime, "Lifetime of the emitter, 0 if infinite. Always emits at least Count particles")
VAR_INFO_ATTRS(fPulsePeriod, "Time between auto-restarts of emitter; 0 if never")
VAR_INFO_ATTRS(fParticleLifeTime, "Lifetime of particles, 0 if indefinite (die with emitter)")
VAR_INFO_ATTRS(bRemainWhileVisible, "Particles will only die when not rendered (by any viewport)")

ATTRS_INFO("<Group=Location>")
VAR_INFO_ATTRS(vLocalSpawnPosOffset, "Spawn offset from the emitter position")
VAR_INFO_ATTRS(vLocalSpawnPosRandomOffset, "Random offset of emission relative position to the spawn position")
VAR_INFO_ATTRS(fLocalOffsetRoundness, "Fraction of emit volume corners to round: 0 = box, 1 = ellipsoid")
VAR_INFO_ATTRS(fLocalSpawnIncrement, "Fraction of inner emit volume to avoid")
VAR_INFO_ATTRS(vSpawnPosOffset, "Spawn offset from the emitter position")
VAR_INFO_ATTRS(fOffsetRoundness, "Fraction of emit volume corners to round: 0 = box, 1 = ellipsoid")
VAR_INFO_ATTRS(fSpawnIncrement, "<SoftMin=0> <SoftMax=1>Fraction of inner emit volume to avoid")
VAR_INFO_ATTRS(eAttachType, "Which geometry to use for attached entity")
VAR_INFO_ATTRS(eAttachForm, "Which aspect of attached geometry to emit from")

ATTRS_INFO("<Group=Angles>")
VAR_INFO_ATTRS(fFocusAngle, "Angle to vary focus from default (Z axis), for variation")
VAR_INFO_ATTRS(fFocusAzimuth, "<SoftMax=360> Angle to rotate focus about default, for variation. 0 = Y axis")
VAR_INFO_ATTRS(fFocusCameraDir, "Rotate emitter focus partially or fully to face camera")
VAR_INFO_ATTRS(bFocusGravityDir, "Uses negative gravity dir, rather than emitter Y, as focus dir")
VAR_INFO_ATTRS(bFocusRotatesEmitter, "Focus rotation is equivalent to emitter rotation; else affects just emission direction")
VAR_INFO_ATTRS(bEmitOffsetDir, "Default emission direction parallel to emission offset from origin")
VAR_INFO_ATTRS(fEmitAngle, "Angle from focus dir (emitter Y), in degrees. RandomVar determines min angle")
VAR_INFO_ATTRS(eFacing, "Orientation of particle face")
VAR_INFO_ATTRS(eFacingGpu, "Orientation of particle face for gpu particles")
VAR_INFO_ATTRS(bOrientToVelocity, "Particle X axis aligned to velocity direction")
VAR_INFO_ATTRS(fCurvature, "For Facing=Camera, fraction that normals are curved to a spherical shape")

ATTRS_INFO("<Group=Appearance>")
VAR_INFO_ATTRS(eBlendType, "Blend rendering type")
VAR_INFO_ATTRS(eSortMethod, "Sorting method for alpha blended particles")
VAR_INFO_ATTRS(fSortConvergancePerFrame, "Sort convergance per frame for odd even sort")
VAR_INFO_ATTRS(sTexture, "Texture asset for sprite")
VAR_INFO_ATTRS(sNormalMap, "Normal texture asset for sprite")
VAR_INFO_ATTRS(sGlowMap, "Glow texture asset for sprite")
VAR_INFO_ATTRS(TextureTiling, "Tiling of texture for animation and variation")
VAR_INFO_ATTRS(GlowTextureTiling, "Tiling of texture for animation and variation")
VAR_INFO_ATTRS(sMaterial, "Material (overrides texture)")
VAR_INFO_ATTRS(bTessellation, "If hardware supports, tessellate particles for better shadowing and curved connected particles")
VAR_INFO_ATTRS(bOctagonalShape, "Use octagonal shape for textures instead of quad")
VAR_INFO_ATTRS(bSoftParticle, "Soft intersection with background")
VAR_INFO_ATTRS(bMotionBlur, "Motion blur for particles")
VAR_INFO_ATTRS(sGeometry, "Geometry for 3D particles")
VAR_INFO_ATTRS(eGeometryPieces, "Which geometry pieces to emit")
VAR_INFO_ATTRS(bNoOffset, "Disable centering of geometry")
VAR_INFO_ATTRS(fAlpha, "<SoftMax=1> Alpha value (opacity, or multiplier for additive)")
VAR_INFO_ATTRS(AlphaClip, "Alpha clipping settings, for particle alpha 0 to 1")
VAR_INFO_ATTRS(cColor, "Color modulation")

ATTRS_INFO("<Group=Lighting>")
VAR_INFO_ATTRS(fDiffuseLighting, "<Max=1000> Multiplier for particle dynamic lighting")
VAR_INFO_ATTRS(fDiffuseBacklighting, "Fraction of diffuse lighting applied in all directions")
VAR_INFO_ATTRS(fEmissiveLighting, "<Max=1000> Multiplier for particle emissive lighting")
VAR_INFO_ATTRS(fEnvProbeLighting, "Scalar of environment probe lighting")
VAR_INFO_ATTRS(bReceiveShadows, "Shadows will cast on these particles")
VAR_INFO_ATTRS(bCastShadows, "Particles will cast shadows (currently only geom particles)")
VAR_INFO_ATTRS(bNotAffectedByFog, "Ignore fog")
VAR_INFO_ATTRS(bGlobalIllumination, "Enable global illumination in the shader")
VAR_INFO_ATTRS(LightSource, "Per-particle light generation")

ATTRS_INFO("<Group=Audio>")
VAR_INFO_ATTRS(sStartTrigger, "Audio start-trigger to execute")
VAR_INFO_ATTRS(sStopTrigger, "Audio stop-trigger to execute")
VAR_INFO_ATTRS(fSoundFXParam, "Custom real-time sound modulation parameter")
VAR_INFO_ATTRS(eSoundControlTime, "The sound control time type")

ATTRS_INFO("<Group=Size>")
VAR_INFO_ATTRS(bMaintainAspectRatio, "Maintain particle aspect ratio")
VAR_INFO_ATTRS(fSizeX, "$<SoftMax=800> X size for sprites, X size scale for geometry")
VAR_INFO_ATTRS(fSizeY, "<ForceUpdate> $<SoftMax=800> Y size for sprites; Y size scale for geometry")
VAR_INFO_ATTRS(fSizeZ, "<ForceUpdate> $<SoftMax=800> Z size scale for geometry")

VAR_INFO_ATTRS(fPivotX, "<SoftMin=-1> <SoftMax=1> Pivot offset in X direction")
VAR_INFO_ATTRS(fPivotY, "<SoftMin=-1> <SoftMax=1> Pivot offset in Y direction")
VAR_INFO_ATTRS(fPivotZ, "<SoftMin=-1> <SoftMax=1> Pivot offset in Z direction of geometry particle")
VAR_INFO_ATTRS(fTailLength, "<SoftMax=10> Length of tail in seconds")
VAR_INFO_ATTRS(fMinPixels, "<SoftMax=10> Augment true size with this many pixels")
VAR_INFO_ATTRS(fLocalStretch, "<SoftMax=1> Stretch particle into moving direction, amount in seconds")
VAR_INFO_ATTRS(fStretch, "<SoftMax=1> Stretch particle into moving direction, amount in seconds")

ATTRS_INFO("<Group=Movement>")
VAR_INFO_ATTRS(bMinVisibleSegmentLength, "Turns on trail particle minimum length to appear")
VAR_INFO_ATTRS(fMinVisibleSegmentLengthFloat, "Trail particle minimum length to appear")
VAR_INFO_ATTRS(fSpeed, "Initial speed of a particle")
VAR_INFO_ATTRS(fInheritVelocity, "<SoftMin=0> $<SoftMax=1> Fraction of emitter's velocity to inherit")
VAR_INFO_ATTRS(fAirResistance, "<SoftMax=10> Air drag value, in inverse seconds")
VAR_INFO_ATTRS(fGravityScale, "<SoftMin=-1> $<SoftMax=1> Multiplier for world gravity")
VAR_INFO_ATTRS(vAcceleration, "Explicit world-space acceleration vector")
VAR_INFO_ATTRS(fTurbulence3DSpeed, "<SoftMax=10> 3D random turbulence force")
VAR_INFO_ATTRS(fTurbulenceSize, "<SoftMax=10> Radius of vortex rotation (axis is direction of movement)")
VAR_INFO_ATTRS(fTurbulenceSpeed, "<SoftMin=-360> $<SoftMax=360> Angular speed of vortex rotation")
VAR_INFO_ATTRS(eMoveRelEmitter, "Particle motion is in emitter space")
VAR_INFO_ATTRS(bBindEmitterToCamera, "Emitter is camera-relative")
VAR_INFO_ATTRS(bSpaceLoop, "Loops particles within emission volume, or within Camera Max Distance")
VAR_INFO(TargetAttraction)

ATTRS_INFO("<Group=Rotation>")
VAR_INFO_ATTRS(vLocalInitAngles, "Initial rotation in symmetric angles (degrees)")
VAR_INFO_ATTRS(vLocalRandomAngles, "Bidirectional random angle variation")
VAR_INFO_ATTRS(vInitAngles, "Initial rotation in symmetric angles (degrees)")
VAR_INFO_ATTRS(vRandomAngles, "Bidirectional random angle variation")
VAR_INFO_ATTRS(vRotationRateX, "<SoftMin=-360> $<SoftMax=360> Rotation speed (degree/sec)")
VAR_INFO_ATTRS(vRotationRateY, "<SoftMin=-360> $<SoftMax=360> Rotation speed (degree/sec)")
VAR_INFO_ATTRS(vRotationRateZ, "<SoftMin=-360> $<SoftMax=360> Rotation speed (degree/sec)")

ATTRS_INFO("<Group=Collision>")
VAR_INFO_ATTRS(eDepthCollision, "What kind of collision computation to perform")
VAR_INFO_ATTRS(fCubemapFarDistance, "$<SoftMin=1> For Cubemap Collision, far distance of the cubemap")
VAR_INFO_ATTRS(ePhysicsType, "What kind of physics simulation to run on particle")
#if !PARTICLES_USE_CRY_PHYSICS
VAR_INFO_ATTRS(sCollisionLayer, "Collision layer for particles")
VAR_INFO_ATTRS(sCollisionGroup, "Collision group for particles to collide with")
#endif
VAR_INFO_ATTRS(bCollideTerrain, "Collides with terrain (if Physics <> none)")
VAR_INFO_ATTRS(bCollideStaticObjects, "Collides with static physics objects (if Physics <> none)")
VAR_INFO_ATTRS(bCollideDynamicObjects, "Collides with dynamic physics objects (if Physics <> none)")
VAR_INFO_ATTRS(fCollisionFraction, "Fraction of emitted particles that actually perform collisions")
VAR_INFO_ATTRS(fCollisionCutoffDistance, "Maximum distance up until collisions are respected (0 = infinite)")
VAR_INFO_ATTRS(nMaxCollisionEvents, "Max # collision events per particle (0 = no limit)")
VAR_INFO_ATTRS(eFinalCollision, "What to do on final collision (when MaxCollisions > 0")
#if PARTICLES_USE_CRY_PHYSICS
VAR_INFO_ATTRS(sSurfaceType, "Surface type for physicalized particles")
#else
VAR_INFO_ATTRS(bUseSelfCollision, "Use collision against particles of the same emitter for die/stop behavior")
VAR_INFO_ATTRS(sPhysicsMaterial, "Physics material to use for particles body (from Default material library only)")
#endif
VAR_INFO_ATTRS(fElasticity, "$<SoftMax=1> Collision bounce coefficient: 0 = no bounce, 1 = full bounce, <0 = die")
VAR_INFO_ATTRS(fDynamicFriction, "<SoftMax=10> Sliding drag value, in inverse seconds")
VAR_INFO_ATTRS(fThickness, "<SoftMax=1> Lying thickness ratio - for physicalized particles only")
VAR_INFO_ATTRS(fDensity, "<SoftMax=2000> Mass density for physicslized particles")


ATTRS_INFO("<Group=Visibility>")
VAR_INFO_ATTRS(bCameraNonFacingFade, "If the particle is close to orthagonal to the camera it will be faded out")
VAR_INFO_ATTRS(fViewDistanceAdjust, "<SoftMax=1> Multiplier to automatic distance fade-out")
VAR_INFO_ATTRS(fCameraMaxDistance, "<SoftMax=100> Max distance from camera to render particles")
VAR_INFO_ATTRS(fCameraMinDistance, "<SoftMax=100> Min distance from camera to render particles")
VAR_INFO_ATTRS(fCameraDistanceOffset, "<SoftMin=-1> <SoftMax=1> Offset the emitter away from the camera")
VAR_INFO_ATTRS(fCameraFadeNearStrength, "Strength of the camera distance fade at the near end")
VAR_INFO_ATTRS(fCameraFadeFarStrength, "Strength of the camera distance fade at the far end")
VAR_INFO_ATTRS(fSortOffset, "<SoftMin=-1> <SoftMax=1> Offset distance used for sorting")
VAR_INFO_ATTRS(fSortBoundsScale, "<SoftMin=-1> <SoftMax=1> Specify point in emitter for sorting; 1 = bounds nearest, 0 = origin, -1 = bounds farthest")
VAR_INFO_ATTRS(bDynamicCulling, "Force enable Dynamic Culling. This disables culling of particle simulation to get accurate bounds for render culling.")
VAR_INFO_ATTRS(bDrawNear, "Render particle in near space (weapon)")
VAR_INFO_ATTRS(bDrawOnTop, "Render particle on top of everything (no depth test)")
VAR_INFO_ATTRS(tVisibleIndoors, "Whether visible indoors / outdoors / both")
VAR_INFO_ATTRS(tVisibleUnderwater, "Whether visible under / above water / both")

ATTRS_INFO("<Group=Advanced>")
VAR_INFO_ATTRS(eForceGeneration, "Generate physical forces if set")
VAR_INFO_ATTRS(fFillRateCost, "Adjustment to max screen fill allowed per emitter")
#ifdef PARTICLE_MOTION_BLUR
VAR_INFO_ATTRS(fMotionBlurScale, "Multiplier for motion blur texture blurring")
VAR_INFO_ATTRS(fMotionBlurStretchScale, "Multiplier for motion blur sprite stretching based on particle movement")
VAR_INFO_ATTRS(fMotionBlurCamStretchScale, "Multiplier for motion blur sprite stretching based on camera movement")
#endif
VAR_INFO_ATTRS(fHeatScale, "Multiplier to thermal vision")
VAR_INFO_ATTRS(fSphericalApproximation, "align the particle to the tangent of the sphere")
VAR_INFO_ATTRS(fPlaneAlignBlendDistance, "<SoftMin=0><SoftMax=100> Distance when blend to camera plane aligned particles starts")

VAR_INFO_ATTRS(nSortQuality, "Sort new particles as accurately as possible into list, by main camera distance")
VAR_INFO_ATTRS(bHalfRes, "Use half resolution rendering")
VAR_INFO_ATTRS(bStreamable, "Texture/geometry allowed to be streamed")
VAR_INFO_ATTRS(bVolumeFog, "Use as a participating media of volumetric fog")
VAR_INFO_ATTRS(fVolumeThickness, "Thickness of participating media, scale for particle size")
VAR_INFO_ATTRS(nParticleSizeDiscard, "Minimum size in pixels of particle, particles smaller or equal too this value will be discarted")
VAR_INFO_ATTRS(DepthOfFieldBlur, "Particles will be blurred against depth of field fullscreen effect. (Excluding geometry and decal types)")
VAR_INFO_ATTRS(FogVolumeShadingQualityHigh, "Particle fog volume shading quality high, fog volumes are handled more accurately.")

ATTRS_INFO("<Group=Configuration>")
VAR_INFO_ATTRS(eConfigMin, "Minimum config spec this effect runs in")
VAR_INFO_ATTRS(eConfigMax, "Maximum config spec this effect runs in")
VAR_INFO_ATTRS(Platforms, "Platforms this effect runs on")
STRUCT_INFO_END(ParticleParams)

STRUCT_INFO_BEGIN(ParticleParams::BoundingVolume)
BASE_INFO(TSmallBool)
VAR_INFO_ATTRS(fX, "0 - Unlimited, otherwise limit to this size along the X axis")
VAR_INFO_ATTRS(fY, "0 - Unlimited, otherwise limit to this size along the Y axis")
VAR_INFO_ATTRS(fZ, "0 - Unlimited, otherwise limit to this size along the Z axis")
STRUCT_INFO_END(ParticleParams::BoundingVolume)

STRUCT_INFO_BEGIN(ParticleParams::SMaintainDensity)
BASE_INFO(UFloatMaintainDensity)
VAR_INFO_ATTRS(fReduceLifeTime, "<SoftMax=1> Reduce life time inversely to count increase")
VAR_INFO_ATTRS(fReduceAlpha, "<SoftMax=1> Reduce alpha inversely to count increase")
VAR_INFO_ATTRS(fReduceSize, "<SoftMax=1> Reduce size inversely to count increase")
STRUCT_INFO_END(ParticleParams::SMaintainDensity)

STRUCT_INFO_BEGIN(ParticleParams::SSoftParticle)
BASE_INFO(TSmallBool)
VAR_INFO_ATTRS(fSoftness, "<SoftMax=2> Soft particle scale")
STRUCT_INFO_END(ParticleParams::SSoftParticle)

STRUCT_INFO_BEGIN(ParticleParams::SMotionBlur)
BASE_INFO(TSmallBool)
VAR_INFO_ATTRS(fBlurStrength, "Strength of motion blur based on object motion")
STRUCT_INFO_END(ParticleParams::SMotionBlur)
STRUCT_INFO_BEGIN(ParticleParams::SAirResistance)
BASE_INFO(TVarEPParam<UFloat> )
VAR_INFO_ATTRS(fRotationalDragScale, "<SoftMax=1> Multiplier to AirResistance, for rotational motion")
VAR_INFO_ATTRS(fWindScale, "<SoftMax=1> Artificial adjustment to environmental wind")
STRUCT_INFO_END(ParticleParams::SAirResistance)


STRUCT_INFO_BEGIN(ParticleParams::SAlphaClip)
VAR_INFO_ATTRS(fScale, "<SoftMax=1> Alpha multiplier for particle alpha 0 to 1")
VAR_INFO_ATTRS(fSourceMin, "Source alpha clip min, for particle alpha 0 to 1")
VAR_INFO_ATTRS(fSourceWidth, "Source alpha clip range, for particle alpha 0 to 1")
STRUCT_INFO_END(ParticleParams::SAlphaClip)

STRUCT_INFO_BEGIN(ParticleParams::SLightSource)
VAR_INFO_ATTRS(bAffectsThisAreaOnly, "Light source affects current clip volume only")
VAR_INFO_ATTRS(fRadius, "<Max=100> Radius of light")
VAR_INFO_ATTRS(fIntensity, "<SoftMax=10> Intensity of light (color from Appearance/Color)")
STRUCT_INFO_END(ParticleParams::SLightSource)

STRUCT_INFO_BEGIN(ParticleParams::SCameraNonFacingFade)
BASE_INFO(TSmallBool)
VAR_INFO_ATTRS(fFadeCurve, "Curve multiplied with how orthogonal the particle plane is to the camera and fades it.")
STRUCT_INFO_END(ParticleParams::SCameraNonFacingFade)

STRUCT_INFO_BEGIN(ParticleParams::SConnection)
BASE_INFO(TSmallBool)
VAR_INFO_ATTRS(bConnectToOrigin, "Newest particle connected to emitter origin")
VAR_INFO_ATTRS(eTextureMapping, "Basis of texture coord mapping (particle or stream)")
VAR_INFO_ATTRS(bTextureMirror, "Mirror alternating texture tiles; else wrap")
VAR_INFO_ATTRS(fTextureFrequency, "<SoftMax=8> Number of texture wraps per particle or sequence")
STRUCT_INFO_END(ParticleParams::SConnection)

STRUCT_INFO_BEGIN(ParticleParams::STargetAttraction)
VAR_INFO_ATTRS(eTarget, "Source of target attractor")
VAR_INFO_ATTRS(bExtendSpeed, "Extend particle speed as necessary to reach target in normal lifetime")
VAR_INFO_ATTRS(bShrink, "Shrink particle as it approaches target")
VAR_INFO_ATTRS(bOrbit, "Orbit target at specified distance, rather than disappearing")
VAR_INFO_ATTRS(fRadius, "Distance from attractor for vanishing or orbiting")
STRUCT_INFO_END(ParticleParams::STargetAttraction)

STRUCT_INFO_BEGIN(ParticleParams::SStretch)
BASE_INFO_ATTRS(TVarEPParam<UFloat>, "<SoftMax=1>")
VAR_INFO_ATTRS(fOffsetRatio, "<Min=-1> $<Max=1> Move particle center this fraction in direction of stretch")
STRUCT_INFO_END(ParticleParams::SStretch)

STRUCT_INFO_BEGIN(ParticleParams::STailLength)
BASE_INFO_ATTRS(TVarEPParam<UFloat>, "<SoftMax=10>")
VAR_INFO_ATTRS(nTailSteps, "<SoftMax=16> Number of tail segments")
STRUCT_INFO_END(ParticleParams::STailLength)

STRUCT_INFO_BEGIN(ParticleParams::SPlatforms)
VAR_INFO(PCDX11)
VAR_INFO(Provo)
ALIAS_INFO_STRINGNAME_ATTR("Xenia", Xenia, "")
VAR_INFO(hasIOS)
VAR_INFO(hasAndroid)
ALIAS_INFO_STRINGNAME_ATTR("MacOS GL", hasMacOSGL, "")
ALIAS_INFO_STRINGNAME_ATTR("MacOS Metal", hasMacOSMetal, "")
STRUCT_INFO_END(ParticleParams::SPlatforms)


#endif // CRYINCLUDE_CRYCOMMON_PARTICLEPARAMS_INFO_H
