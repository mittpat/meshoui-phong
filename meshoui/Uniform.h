#pragma once

#include "gltypes.h"
#include <linalg.h>
#include "hashid.h"

class IUniform
{
public:
    virtual ~IUniform() = 0;
    IUniform();
    IUniform(HashId n);
    virtual const void * data() const = 0;
    virtual void setData(const void * d) = 0;
    HashId name;
};
inline IUniform::~IUniform() {}
inline IUniform::IUniform() {}
inline IUniform::IUniform(HashId n) : name(n) {}

template <int M>
class UniformMfv final
    : public IUniform
{
public:
    virtual ~UniformMfv();
    UniformMfv();
    UniformMfv(HashId n, const linalg::vec<GLfloat, M> & v = linalg::zero);
    virtual const void * data() const override;
    virtual void setData(const void * d) override;

    linalg::vec<GLfloat, M> value;
};
template<int M> inline UniformMfv<M>::~UniformMfv() {}
template<int M> inline UniformMfv<M>::UniformMfv() : IUniform() {}
template<int M> inline UniformMfv<M>::UniformMfv(HashId n, const linalg::vec<GLfloat, M> & v) : IUniform(n), value(v) {}
template<int M> inline const void * UniformMfv<M>::data() const { return &value[0]; }
template<int M> inline void UniformMfv<M>::setData(const void * d) { value = *reinterpret_cast<const linalg::vec<GLfloat, M> *>(d); }

typedef UniformMfv<2> Uniform2fv;
typedef UniformMfv<3> Uniform3fv;
typedef UniformMfv<4> Uniform4fv;

template <int M>
class UniformMMfm final
    : public IUniform
{
public:
    virtual ~UniformMMfm();
    UniformMMfm();
    UniformMMfm(HashId n, const linalg::mat<GLfloat, M, M> & m = linalg::identity);
    virtual const void * data() const override;
    virtual void setData(const void * d) override;

    linalg::mat<GLfloat, M, M> value;
};
template<int M> inline UniformMMfm<M>::~UniformMMfm() {}
template<int M> inline UniformMMfm<M>::UniformMMfm() : IUniform(), value(linalg::identity) {}
template<int M> inline UniformMMfm<M>::UniformMMfm(HashId n, const linalg::mat<GLfloat, M, M> & v) : IUniform(n), value(v) {}
template<int M> inline const void * UniformMMfm<M>::data() const { return &value[0][0]; }
template<int M> inline void UniformMMfm<M>::setData(const void * d) { value = *reinterpret_cast<const linalg::mat<GLfloat, M, M> *>(d); }

typedef UniformMMfm<2> Uniform22fm;
typedef UniformMMfm<3> Uniform33fm;
typedef UniformMMfm<4> Uniform44fm;

template <int M>
class UniformMiv final
    : public IUniform
{
public:
    virtual ~UniformMiv();
    UniformMiv();
    UniformMiv(HashId n, const std::array<GLint, M> & v = std::array<GLint, M>());
    virtual const void * data() const override;
    virtual void setData(const void * d) override;
    void set(const std::string & str);
    void set(const std::string &label, int val, int width = 0);

    std::array<GLint, M> value;
};
template<int M> inline UniformMiv<M>::~UniformMiv() {}
template<int M> inline UniformMiv<M>::UniformMiv() : IUniform() {}
template<int M> inline UniformMiv<M>::UniformMiv(HashId n, const std::array<GLint, M> & v) : IUniform(n), value(v) {}
template<int M> inline const void * UniformMiv<M>::data() const { return &value[0]; }
template<int M> inline void UniformMiv<M>::setData(const void * d) { value = *reinterpret_cast<const std::array<GLint, M> *>(d); }
template<int M> inline void UniformMiv<M>::set(const std::string &str)
{
    std::fill(std::copy(str.begin(), str.end(), value.begin()), value.end(), ' ');
}
template<int M> inline void UniformMiv<M>::set(const std::string &label, int val, int width)
{
    const auto valuestr = std::to_string(val);
    std::fill(std::copy(label.begin(), label.end(), value.begin()), value.end(), ' ');
    std::copy(valuestr.begin(), valuestr.end(), value.begin() + label.size() + std::max((int)valuestr.size(), width) - valuestr.size());
}

class UniformSampler2D final
    : public IUniform
{
public:
    virtual ~UniformSampler2D();
    UniformSampler2D();
    UniformSampler2D(HashId n);
    virtual const void * data() const override;
    virtual void setData(const void *) override;
    std::string filename;
};
inline UniformSampler2D::~UniformSampler2D() {}
inline UniformSampler2D::UniformSampler2D() : IUniform() {}
inline UniformSampler2D::UniformSampler2D(HashId n) : IUniform(n) {}
inline const void * UniformSampler2D::data() const { return nullptr; }
inline void UniformSampler2D::setData(const void *) {}

namespace UniformFactory { IUniform * makeUniform(HashId name, GLenum type, int size = 1); }
