// Single source of truth for "overridable MaterialCB fields".
// Add new fields here when MaterialCB expands (PBR/HDR etc.)
//
// Usage:
//   #define MATERIAL_CB_FIELD_LIST(X) ...
//   MATERIAL_CB_FIELD_LIST(X)
//
// X(key, member, kind)
// - key: json key string
// - member: MaterialCB member name
// - kind: one of Vec4, Vec2, Float, UInt

#pragma once

#define MATERIAL_CB_OVERRIDE_FIELDS(X) \
    X("baseColorTint",        baseColorTint,        Vec4) \
    X("emissiveColor",        emissiveColor,        Vec4) \
    X("emissiveIntensity",    emissiveIntensity,    Float) \
    X("emissiveMaskMul",      emissiveMaskMul,      Float) \
    X("normalStrength",       normalStrength,       Float) \
    X("opacityMaskClipValue", opacityMaskClipValue, Float) \
    X("aoScale",              aoScale,              Float) \
    X("roughnessScale",       roughnessScale,       Float) \
    X("metalnessScale",       metalnessScale,       Float) \
    X("ormAoSource",          ormAoSource,          UInt) \
    X("ormRoughnessSource",   ormRoughnessSource,   UInt) \
    X("ormMetalnessSource",   ormMetalnessSource,   UInt) \
    X("opacityMaskSource",    opacityMaskSource,    UInt) \
    X("ormAoSourceSlot",      ormAoSourceSlot,      UInt) \
    X("ormRoughnessSourceSlot", ormRoughnessSourceSlot, UInt) \
    X("ormMetalnessSourceSlot", ormMetalnessSourceSlot, UInt) \
    X("uvScale",              uvScale,              Vec2) \
    X("uvOffset",             uvOffset,             Vec2) \
    X("uvRotation",           uvRotation,           Float) \
    X("materialMask",         materialMask,         UInt)

