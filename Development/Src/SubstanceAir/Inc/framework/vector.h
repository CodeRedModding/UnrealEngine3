//! @file vector.h
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_FRAMEWORK_VECTOR_H
#define _SUBSTANCE_AIR_FRAMEWORK_VECTOR_H

#include <ostream>
#include <istream>
#include <string>


namespace SubstanceAir
{

namespace Math
{

template <typename T,int N>
struct Vector
{
	typedef T Type;
	static const int Size = N;
	T x;                              //!< First component of the vector
	T& operator[](size_t i) { return (&x)[i]; }
	const T& operator[](size_t i) const { return (&x)[i]; }

	friend inline std::ostream& operator<<(std::ostream& os,const Vector<T,N>& v)
	{
		for (size_t k=0;k<N;++k)
		{
			if (k!=0)
			{
				os << ",";
			}
			os << v[k];
		}
		return os;
	}

	friend inline std::istream& operator>>(std::istream& is,Vector<T,N>& v)
	{
		for (size_t k=0;k<N;++k)
		{
			std::string elem;
			getline(is,elem,',');
			v[k] = boost::lexical_cast<T>(elem);
		}
		return is;
	}
	
	friend inline bool operator==(const Vector<T,N>& a,const Vector<T,N>& b)
	{
		for (size_t k=0;k<N;++k)
		{
			if (a[k]!=b[k])
				return false;
		}
		return true;
	}
	
	friend inline bool operator!=(const Vector<T,N>& a,const Vector<T,N>& b)
	{
		return !(a==b);
	}
};

template <typename T> struct Vector2 : public Vector<T,2>
{
	Vector2() {}
	Vector2(T _x,T _y) : y(_y) { this->x=_x; }
	T y;
};

template <typename T> struct Vector3 : public Vector<T,3>
{
	Vector3() {}
	Vector3(T _x,T _y,T _z) : y(_y),z(_z) { this->x=_x; }
	T y,z;
};

template <typename T> struct Vector4 : public Vector<T,4>
{
	Vector4() {}
	Vector4(T _x,T _y,T _z,T _w) : y(_y),z(_z),w(_w) { this->x=_x; }
	T y,z,w;
};

}  // namespace Math


}  // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_FRAMEWORK_VECTOR_H
