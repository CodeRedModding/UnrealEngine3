/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include <assert.h>

#include <math.h>



float frand( const float fMin=0, const float fMax=1 )
{
	return fMin + (rand() % 1024) / 1024.0f * (fMax - fMin);			// crappy implementation
}

float frand2()
{
	return frand()*2.0f-1.0f;
}

float saturate( const float x )
{
	if(x<0.0f)
		return 0.0f;

	if(x>1.0f)
		return 1.0f;

	return x;
}

struct vec2
{
	float x,y;

	vec2(){}

	vec2( float _x, float _y ) :x(_x),y(_y) {}

	float length2() const
	{
		return x*x+y*y;
	}

	float length() const
	{
		return (float)sqrt(length2());
	}

	vec2 operator-( const vec2 &rhs ) const
	{
		return vec2(x-rhs.x,y-rhs.y);
	}

	vec2 operator+( const vec2 &rhs ) const
	{
		return vec2(x+rhs.x,y+rhs.y);
	}

	const vec2 &operator+=( const vec2 &rhs )
	{
		*this = *this + rhs;

		return *this;
	}

	const vec2 &operator*=( const float rhs )
	{
		*this = *this * rhs;

		return *this;
	}

	const vec2 &operator/=( const float rhs )
	{
		*this = *this / rhs;

		return *this;
	}

	const vec2 &operator-=( const vec2 &rhs )
	{
		*this = *this - rhs;

		return *this;
	}

	vec2 operator*( const vec2 &rhs ) const
	{
		return vec2(x*rhs.x,y*rhs.y);
	}

	vec2 operator*( const float rhs ) const
	{
		return vec2(x*rhs,y*rhs);
	}

	vec2 operator/( const float rhs ) const
	{
		return *this * (1.0f/rhs);
	}

	vec2 UnsafeNormal() const
	{
		vec2 ret;

		return *this / length();
	}
};

float dot( const vec2 &a, const vec2 &b )
{
	return a.x*b.x+a.y*b.y;
}







// clip into (0,0) - (1,1)
bool Clip( vec2 &a, vec2 &b )
{
	vec2 vLineMin, vLineMax;

	vLineMin = vec2(min(a.x,b.x),min(a.y,b.y));	vLineMax = vec2(max(a.x,b.x),max(a.y,b.y));

	// left
	if(vLineMax.x<=0.0f)
		return false;

	if(vLineMin.x<0.0f && 0.0f<vLineMax.x)
	{
		float f = a.y + (b.y-a.y)*a.x/(a.x-b.x);
		if(a.x<0.0f)
		{
			// a is clipped
			a.y = f;
			a.x=0;
		}
		else
		{
			// b is clipped
			b.y = f;
			b.x=0;
		}
	}

	vLineMin = vec2(min(a.x,b.x),min(a.y,b.y));	vLineMax = vec2(max(a.x,b.x),max(a.y,b.y));

	// right
	if(vLineMin.x>=1.0f)
		return false;

	if(vLineMin.x<1.0f && 1.0f<vLineMax.x)
	{
		float f = a.y + (1.0f-a.x)/(b.x-a.x)*(b.y-a.y);
		if(a.x>1.0f)
		{
			// a is clipped
			a.y = f;
			a.x=1.0f;
		}
		else
		{
			// b is clipped
			b.y = f;
			b.x=1.0f;
		}
	}

	vLineMin = vec2(min(a.x,b.x),min(a.y,b.y));	vLineMax = vec2(max(a.x,b.x),max(a.y,b.y));

	// top
	if(vLineMax.y<=0.0f)
		return false;

	if(vLineMin.y<0.0f && 0.0f<vLineMax.y)
	{
		float f = a.x + (b.x-a.x)*a.y/(a.y-b.y);
		if(a.y<0.0f)
		{
			// a is clipped
			a.x = f;
			a.y=0;
		}
		else
		{
			// b is clipped
			b.x = f;
			b.y=0;
		}
	}

	vLineMin = vec2(min(a.x,b.x),min(a.y,b.y));	vLineMax = vec2(max(a.x,b.x),max(a.y,b.y));

	// bottom
	if(vLineMin.y>=1.0f)
		return false;

	if(vLineMin.y<1.0f && 1.0f<vLineMax.y)
	{
		float f = a.x + (1.0f-a.y)/(b.y-a.y)*(b.x-a.x);
		if(a.y>1.0f)
		{
			// a is clipped
			a.x = f;
			a.y=1.0f;
		}
		else
		{
			// b is clipped
			b.x = f;
			b.y=1.0f;
		}
	}

	return true;
}





struct vec3
{
	float x,y,z;

	vec3(){}

	vec3( float _x, float _y, float _z ) :x(_x),y(_y),z(_z) {}

	float length2() const
	{
		return x*x+y*y+z*z;
	}

	float length() const
	{
		return (float)sqrt(length2());
	}

	vec3 operator-( const vec3 &rhs ) const
	{
		return vec3(x-rhs.x,y-rhs.y,z-rhs.z);
	}

	vec3 operator+( const vec3 &rhs ) const
	{
		return vec3(x+rhs.x,y+rhs.y,z+rhs.z);
	}

	const vec3 &operator+=( const vec3 &rhs )
	{
		*this = *this + rhs;

		return *this;
	}

	const vec3 &operator*=( const float rhs )
	{
		*this = *this * rhs;

		return *this;
	}

	const vec3 &operator/=( const float rhs )
	{
		*this = *this / rhs;

		return *this;
	}

	const vec3 &operator-=( const vec3 &rhs )
	{
		*this = *this - rhs;

		return *this;
	}

	vec3 operator*( const vec3 &rhs ) const
	{
		return vec3(x*rhs.x,y*rhs.y,z*rhs.z);
	}

	vec3 operator*( const float rhs ) const
	{
		return vec3(x*rhs,y*rhs,z*rhs);
	}

	vec3 operator/( const float rhs ) const
	{
		return *this * (1.0f/rhs);
	}

	vec3 UnsafeNormal() const
	{
		return *this / length();
	}
};

float dot( const vec3 &a, const vec3 &b )
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

vec3 cross( const vec3 &a, const vec3 &b )
{
	return vec3(a.x*b.y-a.y*b.x, 
				a.y*b.z-a.z*b.y, 
				a.z*b.x-a.x*b.z);
}


vec3 frand_PointOnUnitSphere()
{
	for(;;)
	{
		vec3 ret;

		ret.x = frand2();
		ret.y = frand2();
		ret.z = frand2();

		float fL = ret.length();

		if(fL>0.01f && fL<1.0f)
		{
			ret = ret.UnsafeNormal();
			return ret;
		}
	}
}

vec3 frand_PointInUnitSphere()
{
	for(;;)
	{
		vec3 ret;

		ret.x = frand2();
		ret.y = frand2();
		ret.z = frand2();

		float fL = ret.length();

		if(fL<1.0f)
			return ret;
	}
}


// vNormal needs to be normalized
vec3 mirror( const vec3 vP, const vec3 vNormal ) 
{
	return vP - vNormal*2.0f*dot(vNormal,vP);
}




struct vec4
{
	float x,y,z,w;

	vec4(){}

	vec4( const vec3 &p, float _w ) :x(p.x),y(p.y),z(p.z),w(_w) {}

	vec4( float _x, float _y, float _z, float _w ) :x(_x),y(_y),z(_z),w(_w) {}
};
