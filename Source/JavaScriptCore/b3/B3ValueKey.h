/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(B3_JIT)

#include "B3HeapRange.h"
#include "B3Kind.h"
#include "B3Origin.h"
#include "B3Type.h"
#include <wtf/HashTable.h>

namespace JSC { namespace B3 {

class Procedure;
class Value;

// ValueKeys are useful for CSE. They abstractly describe the value that a Value returns when it
// executes. Any Value that has the same ValueKey is guaranteed to return the same value, provided
// that they return a non-empty ValueKey. Operations that have effects, or that can have their
// behavior affected by other operations' effects, will return an empty ValueKey. You have to use
// other mechanisms for doing CSE for impure operations.

class ValueKey {
public:
    ValueKey()
    {
    }

    ValueKey(Kind kind, Type type)
        : m_kind(kind)
        , m_type(type)
    {
    }

    ValueKey(Kind, Type, Value* child);

    ValueKey(Kind, Type, Value* left, Value* right);

    ValueKey(Kind, Type, Value* a, Value* b, Value* c);

    ValueKey(Kind kind, Type type, int64_t value)
        : m_kind(kind)
        , m_type(type)
    {
        u.value = value;
    }

    ValueKey(Kind kind, Type type, double value)
        : m_kind(kind)
        , m_type(type)
    {
        u.doubleValue = value;
    }

    ValueKey(Kind kind, Type type, float value)
        : m_kind(kind)
        , m_type(type)
    {
        // This means that upper 32bit of u.value is 0.
        u.floatValue = value;
    }

    ValueKey(Kind kind, Type type, v128_t value)
        : m_kind(kind)
        , m_type(type)
    {
        u.vectorValue = value;
    }

    ValueKey(Kind, Type, SIMDInfo, Value*);
    ValueKey(Kind, Type, SIMDInfo, Value*, Value*);
    ValueKey(Kind, Type, SIMDInfo, Value*, Value*, Value*);
    ValueKey(Kind, Type, SIMDInfo, Value*, uint8_t);
    ValueKey(Kind, Type, SIMDInfo, Value*, Value*, uint8_t);

    static ValueKey intConstant(Type type, int64_t value);

    SIMDInfo simdInfo() const { return m_simdInfo; }
    Kind kind() const { return m_kind; }
    Opcode opcode() const { return kind().opcode(); }
    Type type() const { return m_type; }
    unsigned childIndex(unsigned index) const { return u.indices[index]; }
    Value* child(Procedure&, unsigned index) const;
    int64_t value() const { return u.value; }
    double doubleValue() const { return u.doubleValue; }
    float floatValue() const { return u.floatValue; }
    v128_t vectorValue() const { return u.vectorValue; }

    bool operator==(const ValueKey& other) const
    {
        return m_simdInfo == other.m_simdInfo
            && m_kind == other.m_kind
            && m_type == other.m_type
            && u == other.u;
    }

    bool operator!=(const ValueKey& other) const
    {
        return !(*this == other);
    }

    unsigned hash() const
    {
        return m_kind.hash() + m_type.hash() + WTF::IntHash<int32_t>::hash(u.indices[0]) + u.indices[1] + u.indices[2] + u.indices[3];
    }

    explicit operator bool() const { return *this != ValueKey(); }

    void dump(PrintStream&) const;

    bool canMaterialize() const
    {
        if (!*this)
            return false;
        switch (opcode()) {
        case CheckAdd:
        case CheckSub:
        case CheckMul:
            return false;
        default:
            return true;
        }
    }

    bool isConstant() const
    {
        return B3::isConstant(opcode());
    }

    // Attempts to materialize the Value for this ValueKey. May return nullptr if the value cannot
    // be materialized. This happens for CheckAdd and friends. You can use canMaterialize() to check
    // if your key is materializable.
    Value* materialize(Procedure&, Origin) const;

    ValueKey(WTF::HashTableDeletedValueType)
        : m_type { Int32 }
    {
    }

    bool isHashTableDeletedValue() const
    {
        return *this == ValueKey(WTF::HashTableDeletedValue);
    }
        
private:
    SIMDInfo m_simdInfo { };
    Kind m_kind { };
    Type m_type { Void };
    union U {
        unsigned indices[4];
        int64_t value;
        double doubleValue;
        float floatValue;
        v128_t vectorValue;

        U()
        {
            indices[0] = 0;
            indices[1] = 0;
            indices[2] = 0;
            indices[3] = 0;
        }

        bool operator==(const U& other) const
        {
            return indices[0] == other.indices[0]
                && indices[1] == other.indices[1]
                && indices[2] == other.indices[2]
                && indices[3] == other.indices[3];
        }
    } u;
};

struct ValueKeyHash {
    static unsigned hash(const ValueKey& key) { return key.hash(); }
    static bool equal(const ValueKey& a, const ValueKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::B3

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::B3::ValueKey> : JSC::B3::ValueKeyHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::B3::ValueKey> : public SimpleClassHashTraits<JSC::B3::ValueKey> {
    static constexpr bool emptyValueIsZero = false;
};

} // namespace WTF

#endif // ENABLE(B3_JIT)
