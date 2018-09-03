#pragma once

#include "gltypes.h"
#include <linalg.h>
#include "hashid.h"

class IGraphicsUniform
{
public:
    virtual ~IGraphicsUniform() = 0;
    IGraphicsUniform();
    IGraphicsUniform(HashId n);
    virtual const void * data() const = 0;
    virtual void setData(const void * d) = 0;
    HashId name;
};
inline IGraphicsUniform::~IGraphicsUniform() {}
inline IGraphicsUniform::IGraphicsUniform() {}
inline IGraphicsUniform::IGraphicsUniform(HashId n) : name(n) {}

template <int M>
class GraphicsUniformMfv final
    : public IGraphicsUniform
{
public:
    virtual ~GraphicsUniformMfv();
    GraphicsUniformMfv();
    GraphicsUniformMfv(HashId n, const linalg::vec<GLfloat, M> & v = linalg::zero);
    virtual const void * data() const override;
    virtual void setData(const void * d) override;

    linalg::vec<GLfloat, M> value;
};
template<int M> inline GraphicsUniformMfv<M>::~GraphicsUniformMfv() {}
template<int M> inline GraphicsUniformMfv<M>::GraphicsUniformMfv() : IGraphicsUniform() {}
template<int M> inline GraphicsUniformMfv<M>::GraphicsUniformMfv(HashId n, const linalg::vec<GLfloat, M> & v) : IGraphicsUniform(n), value(v) {}
template<int M> inline const void * GraphicsUniformMfv<M>::data() const { return &value[0]; }
template<int M> inline void GraphicsUniformMfv<M>::setData(const void * d) { value = *reinterpret_cast<const linalg::vec<GLfloat, M> *>(d); }

typedef GraphicsUniformMfv<2> GraphicsUniform2fv;
typedef GraphicsUniformMfv<3> GraphicsUniform3fv;
typedef GraphicsUniformMfv<4> GraphicsUniform4fv;

template <int M>
class GraphicsUniformMMfm final
    : public IGraphicsUniform
{
public:
    virtual ~GraphicsUniformMMfm();
    GraphicsUniformMMfm();
    GraphicsUniformMMfm(HashId n, const linalg::mat<GLfloat, M, M> & m = linalg::identity);
    virtual const void * data() const override;
    virtual void setData(const void * d) override;

    linalg::mat<GLfloat, M, M> value;
};
template<int M> inline GraphicsUniformMMfm<M>::~GraphicsUniformMMfm() {}
template<int M> inline GraphicsUniformMMfm<M>::GraphicsUniformMMfm() : IGraphicsUniform(), value(linalg::identity) {}
template<int M> inline GraphicsUniformMMfm<M>::GraphicsUniformMMfm(HashId n, const linalg::mat<GLfloat, M, M> & v) : IGraphicsUniform(n), value(v) {}
template<int M> inline const void * GraphicsUniformMMfm<M>::data() const { return &value[0][0]; }
template<int M> inline void GraphicsUniformMMfm<M>::setData(const void * d) { value = *reinterpret_cast<const linalg::mat<GLfloat, M, M> *>(d); }

typedef GraphicsUniformMMfm<2> GraphicsUniform22fm;
typedef GraphicsUniformMMfm<3> GraphicsUniform33fm;
typedef GraphicsUniformMMfm<4> GraphicsUniform44fm;

template <int M>
class GraphicsUniformMiv final
    : public IGraphicsUniform
{
public:
    virtual ~GraphicsUniformMiv();
    GraphicsUniformMiv();
    GraphicsUniformMiv(HashId n, const std::array<GLint, M> & v = std::array<GLint, M>());
    virtual const void * data() const override;
    virtual void setData(const void * d) override;
    void set(const std::string & str);
    void set(const std::string &label, int val, int width = 0);

    std::array<GLint, M> value;
};
template<int M> inline GraphicsUniformMiv<M>::~GraphicsUniformMiv() {}
template<int M> inline GraphicsUniformMiv<M>::GraphicsUniformMiv() : IGraphicsUniform() {}
template<int M> inline GraphicsUniformMiv<M>::GraphicsUniformMiv(HashId n, const std::array<GLint, M> & v) : IGraphicsUniform(n), value(v) {}
template<int M> inline const void * GraphicsUniformMiv<M>::data() const { return &value[0]; }
template<int M> inline void GraphicsUniformMiv<M>::setData(const void * d) { value = *reinterpret_cast<const std::array<GLint, M> *>(d); }
template<int M> inline void GraphicsUniformMiv<M>::set(const std::string &str)
{
    std::fill(std::copy(str.begin(), str.end(), value.begin()), value.end(), ' ');
}
template<int M> inline void GraphicsUniformMiv<M>::set(const std::string &label, int val, int width)
{
    const auto valuestr = std::to_string(val);
    std::fill(std::copy(label.begin(), label.end(), value.begin()), value.end(), ' ');
    std::copy(valuestr.begin(), valuestr.end(), value.begin() + label.size() + std::max((int)valuestr.size(), width) - valuestr.size());
}

typedef GraphicsUniformMiv<16> GraphicsLabel;

class GraphicsUniformSampler2D final
    : public IGraphicsUniform
{
public:
    virtual ~GraphicsUniformSampler2D();
    GraphicsUniformSampler2D();
    GraphicsUniformSampler2D(HashId n);
    virtual const void * data() const override;
    virtual void setData(const void *) override;
    std::string filename;
};
inline GraphicsUniformSampler2D::~GraphicsUniformSampler2D() {}
inline GraphicsUniformSampler2D::GraphicsUniformSampler2D() : IGraphicsUniform() {}
inline GraphicsUniformSampler2D::GraphicsUniformSampler2D(HashId n) : IGraphicsUniform(n) {}
inline const void * GraphicsUniformSampler2D::data() const { return nullptr; }
inline void GraphicsUniformSampler2D::setData(const void *) {}

namespace GraphicsUniformFactory { IGraphicsUniform * makeUniform(HashId name, GLenum type, int size = 1); }
