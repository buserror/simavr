/*
	c3algebra.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

	Derivative and inspiration from original C++:
	Paul Rademacher & Jean-Francois DOUEG,

 	This file is part of libc3.

	libc3 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	libc3 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with libc3.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __C3ALGEBRA_H___
#define __C3ALGEBRA_H___


#ifndef M_PI
#define M_PI 3.141592654
#endif
#define PI_OVER_180	 0.017453292519943295769236907684886
#define PI_OVER_360	 0.0087266462599716478846184538424431

enum {VX, VY, VZ, VW};           // axes
enum {PA, PB, PC, PD};           // planes
enum {RED, GREEN, BLUE, ALPHA};  // colors
enum {KA, KD, KS, ES};           // phong coefficients

typedef float c3f;
typedef c3f (*V_FCT_PTR)(c3f);

typedef union c3vec2 {
	struct { c3f x,y; };
	c3f n[2];
} c3vec2;

typedef union c3vec3 {
	struct { c3f x,y,z; };
	c3f n[3];
} c3vec3;

typedef union c3vec4 {
	struct { c3f x,y,z,w; };
	c3f n[4];
} c3vec4, * c3vec4p;

typedef union c3mat3 {
	c3vec3 v[3];
	c3f n[3*3];
} c3mat3, * c3mat3p;

typedef union c3mat4 {
	c3vec4 v[4];
	c3f n[4*4];
} c3mat4, * c3mat4p;

/*
 * c3vec2 related
 */

c3vec2	c3vec2_zero();
c3vec2	c3vec2f(c3f x, c3f y);

c3vec2	c3vec2_add(c3vec2 a, const c3vec2 v);
c3vec2	c3vec2_sub(c3vec2 a, const c3vec2 v);
c3vec2	c3vec2_mulf(c3vec2 a, c3f d);
c3vec2	c3vec2_divf(c3vec2 a, c3f d);

c3f		c3vec2_length2(const c3vec2 a);
c3f		c3vec2_length(const c3vec2 a);
c3vec2	c3vec2_normalize(const c3vec2 a); // it is up to caller to avoid divide-by-zero
c3vec2	c3vec2_apply(c3vec2 a, V_FCT_PTR fct);
c3vec2	c3vec2_minus(const c3vec2 a);
c3f		c3vec2_dot(const c3vec2 a, const c3vec2 b);
c3vec2	c3vec2_min(const c3vec2 a, const c3vec2 b);
c3vec2	c3vec2_max(const c3vec2 a, const c3vec2 b);
c3vec2	c3vec2_prod(const c3vec2 a, const c3vec2 b);

/*
 * c3vec3 related
 */

c3vec3	c3vec3_zero();
c3vec3	c3vec3f(c3f x, c3f y, c3f z);
c3vec3	c3vec3_vec2f(const c3vec2 v, c3f d);
c3vec3	c3vec3_vec2(const c3vec2 v);
c3vec3	c3vec3_vec4(const c3vec4 v); // it is up to caller to avoid divide-by-zero

c3vec3	c3vec3_add(const c3vec3 a, const c3vec3 v);
c3vec3	c3vec3_sub(const c3vec3 a, const c3vec3 v);
c3vec3	c3vec3_mulf(const c3vec3 a, c3f d);
c3vec3	c3vec3_divf(const c3vec3 a, c3f d);

c3f		c3vec3_length2(const c3vec3 a);
c3f		c3vec3_length(const c3vec3 a);
c3vec3	c3vec3_normalize(const c3vec3 a); // it is up to caller to avoid divide-by-zero
c3vec3	c3vec3_homogenize(c3vec3 a); // it is up to caller to avoid divide-by-zero
c3vec3	c3vec3_apply(c3vec3 a, V_FCT_PTR fct);
c3vec3	c3vec3_minus(const c3vec3 a);
c3f		c3vec3_dot(const c3vec3 a, const c3vec3 b);
int		c3vec3_equal(const c3vec3 a, const c3vec3 b);
c3vec3	c3vec3_min(const c3vec3 a, const c3vec3 b);
c3vec3	c3vec3_max(const c3vec3 a, const c3vec3 b);
c3vec3	c3vec3_prod(const c3vec3 a, const c3vec3 b);

c3vec3	c3vec3_cross(const c3vec3 a, const c3vec3 b);
c3vec3	c3vec2_cross(const c3vec2 a, const c3vec2 b);
c3vec3 	c3vec3_polar(const c3vec3 a); // returns phi, theta, length

/*
 * c3vec4 related
 */

c3vec4	c3vec4_zero();
c3vec4	c3vec4f(c3f x, c3f y, c3f z, c3f w);
c3vec4	c3vec4_vec3(const c3vec3 v);
c3vec4	c3vec4_vec3f(const c3vec3 v, c3f d);

c3vec4	c3vec4_add(c3vec4 a, const c3vec4 v);
c3vec4	c3vec4_sub(c3vec4 a, const c3vec4 v);
c3vec4	c3vec4_mulf(c3vec4 a, c3f d);
c3vec4	c3vec4_divf(c3vec4 a, c3f d);

c3f		c3vec4_length2(const c3vec4 a);
c3f		c3vec4_length(const c3vec4 a);
c3vec4	c3vec4_normalize(c3vec4 a); // it is up to caller to avoid divide-by-zero
c3vec4	c3vec4_homogenize(c3vec4 a); // it is up to caller to avoid divide-by-zero
c3vec4	c3vec4_apply(c3vec4 a, V_FCT_PTR fct);
c3vec4	c3vec4_minus(const c3vec4 a);
int		c3vec4_equal(const c3vec4 a, const c3vec4 b);
c3vec4	c3vec4_min(const c3vec4 a, const c3vec4 b);
c3vec4	c3vec4_max(const c3vec4 a, const c3vec4 b);
c3vec4	c3vec4_prod(const c3vec4 a, const c3vec4 b);

/*
 * c3mat3 related
 */

c3mat3	c3mat3_identity();
c3mat3	c3mat3_vec3(const c3vec3 v0, const c3vec3 v1, const c3vec3 v2);
c3mat3p	c3mat3_add(const c3mat3p a, const c3mat3p m);
c3mat3p	c3mat3_sub(const c3mat3p a, const c3mat3p m);
c3mat3p	c3mat3_mulf(const c3mat3p a, c3f d);
c3mat3p	c3mat3_divf(const c3mat3p a, c3f d);

c3mat3	c3mat3_transpose(const c3mat3p a);
c3mat3	c3mat3_inverse(const c3mat3p m);   // Gauss-Jordan elimination with partial pivoting
c3mat3p	c3mat3_apply(c3mat3p a, V_FCT_PTR fct);
c3mat3	c3mat3_minus(const c3mat3p a);

c3mat3	c3mat3_mul(const c3mat3p a, const c3mat3p b);
int		c3mat3_equal(const c3mat3p a, const c3mat3p b);

c3vec2	c3mat3_mulv2(const c3mat3p a, const c3vec2 v);
c3vec3	c3mat3_mulv3(const c3mat3p a, const c3vec3 v);
c3vec2	c3vec2_mulm3(const c3vec2 v, const c3mat3p a);
c3vec3	c3vec3_mulm3(const c3vec3 v, const c3mat3p a);

c3mat3	identity2D();
c3mat3	translation2D(const c3vec2 v);
c3mat3	rotation2D(const c3vec2 Center, c3f angleDeg);
c3mat3	scaling2D(const c3vec2 scaleVector);

/*
 * c3mat4 related
 */

c3mat4	c3mat4_identity();
c3mat4	c3mat4_vec4(const c3vec4 v0, const c3vec4 v1, const c3vec4 v2, const c3vec4 v3);
c3mat4	c3mat4f(
     c3f a00, c3f a01, c3f a02, c3f a03,
     c3f a10, c3f a11, c3f a12, c3f a13,
     c3f a20, c3f a21, c3f a22, c3f a23,
     c3f a30, c3f a31, c3f a32, c3f a33 );

c3mat4	c3mat4_minus(const c3mat4p a);
c3mat4p	c3mat4p_add(c3mat4p a, const c3mat4p m);
c3mat4	c3mat4_add(const c3mat4p a, const c3mat4p b);
c3mat4p	c3mat4p_sub(c3mat4p a, const c3mat4p m);
c3mat4	c3mat4_sub(const c3mat4p a, const c3mat4p b);
c3mat4p	c3mat4p_mulf(c3mat4p a, c3f d);
c3mat4 	c3mat4_mulf(const c3mat4p a, c3f d);
c3mat4	c3mat4_mul(const c3mat4p a, const c3mat4p b);
c3mat4p	c3mat4p_divf(c3mat4p a, c3f d);
c3mat4	c3mat4_divf(const c3mat4p a, c3f d);

c3mat4	c3mat4_transpose(const c3mat4p a);
c3mat4	c3mat4_inverse(const c3mat4p m);       // Gauss-Jordan elimination with partial pivoting
c3mat4p	c3mat4p_apply(c3mat4p a, V_FCT_PTR fct);
c3mat4p	c3mat4p_swap_rows(c3mat4p a, int i, int j);
c3mat4p	c3mat4p_swap_cols(c3mat4p a, int i, int j);
int		c3mat4_equal(const c3mat4p a, const c3mat4p b);

c3vec4	c3vec4_mulm4(const c3vec4 v, const c3mat4p a);
c3vec4	c3mat4_mulv4(const c3mat4p a, const c3vec4 v);
c3vec3	c3mat4_mulv3(const c3mat4p a, const c3vec3 v);

c3mat4	identity3D();
c3mat4	translation3D(const c3vec3 v);
c3mat4	rotation3D(const c3vec3 Axis, c3f angleDeg);
c3mat4	rotation3Drad(const c3vec3 Axis, c3f angleRad);
c3mat4	scaling3D(const c3vec3 scaleVector);
c3mat4	frustum3D(
			c3f left, c3f right, c3f bottom, c3f top,
			c3f znear, c3f zfar);
c3mat4	perspective3D(c3f fov, c3f aspect, c3f znear, c3f zfar);

#endif /* __C3ALGEBRA_H___ */
