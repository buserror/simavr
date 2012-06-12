/*
	c3algebra.c

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

#include <math.h>
#include <string.h>
#include "c3algebra.h"

#ifndef MAX
#define MAX(a,b)  ((a)>(b) ? (a) : (b))
#define MIN(a,b)  ((a)<(b) ? (a) : (b))
#endif

/****************************************************************
 *                                                              *
 *          c3vec2 Member functions                             *
 *                                                              *
 ****************************************************************/

/******************** c3vec2 CONSTRUCTORS ********************/

c3vec2 c3vec2_zero()
{
	c3vec2 n;// = { .x = 0, .y = 0 };// older gcc <4.6 doesn't like this
	n.x = n.y = 0;
    return n;
}

c3vec2 c3vec2f(c3f x, c3f y)
{
	c3vec2 v;// = { .x = x, .y = y };// older gcc <4.6 doesn't like this
	v.x = x; v.y = y;
	return v;
}

/******************** c3vec2 ASSIGNMENT OPERATORS ******************/

c3vec2  c3vec2_add(c3vec2 a, const c3vec2 v)
{
    a.n[VX] += v.n[VX];
    a.n[VY] += v.n[VY];
    return a;
}

c3vec2  c3vec2_sub(c3vec2 a, const c3vec2 v)
{
    a.n[VX] -= v.n[VX];
    a.n[VY] -= v.n[VY];
    return a;
}

c3vec2 c3vec2_mulf(c3vec2 a, c3f d)
{
    a.n[VX] *= d;
    a.n[VY] *= d;
    return a;
}

c3vec2 c3vec2_divf(c3vec2 a, c3f d)
{
    c3f d_inv = 1.0f/d;
    a.n[VX] *= d_inv;
    a.n[VY] *= d_inv;
    return a;
}

/******************** c3vec2 SPECIAL FUNCTIONS ********************/


c3f c3vec2_length2(const c3vec2 a)
{
    return a.n[VX]*a.n[VX] + a.n[VY]*a.n[VY];
}

c3f c3vec2_length(const c3vec2 a)
{
    return (c3f) sqrt(c3vec2_length2(a));
}

c3vec2 c3vec2_normalize(const c3vec2 a) // it is up to caller to avoid divide-by-zero
{
    return c3vec2_divf(a, c3vec2_length(a));
}

c3vec2 c3vec2_apply(c3vec2 a, V_FCT_PTR fct)
{
    a.n[VX] = fct(a.n[VX]);
    a.n[VY] = fct(a.n[VY]);
    return a;
}


/******************** c3vec2 FRIENDS *****************************/

c3vec2 c3vec2_minus(const c3vec2 a)
{
    return c3vec2f(-a.n[VX],-a.n[VY]);
}

c3vec2 c3mat3_mulv2(const c3mat3p a, const c3vec2 v)
{
  c3vec2 av;

  av.n[VX] = a->v[0].n[VX]*v.n[VX] + a->v[0].n[VY]*v.n[VY] + a->v[0].n[VZ];
  av.n[VY] = a->v[1].n[VX]*v.n[VX] + a->v[1].n[VY]*v.n[VY] + a->v[1].n[VZ];
//  av.n[VZ] = a.v[2].n[VX]*v.n[VX] + a.v[2].n[VY]*v.n[VY] + a.v[2].n[VZ];

  return av;
}

c3vec2 c3vec2_mulm3(const c3vec2 v, const c3mat3p a)
{
	c3mat3 t = c3mat3_transpose(a);
    return c3mat3_mulv2(&t, v);
}

c3vec3 c3mat3_mulv3(const c3mat3p a, const c3vec3 v)
{
    c3vec3 av;

    av.n[VX] = a->v[0].n[VX]*v.n[VX] + a->v[0].n[VY]*v.n[VY] + a->v[0].n[VZ]*v.n[VZ];
    av.n[VY] = a->v[1].n[VX]*v.n[VX] + a->v[1].n[VY]*v.n[VY] + a->v[1].n[VZ]*v.n[VZ];
    av.n[VZ] = a->v[2].n[VX]*v.n[VX] + a->v[2].n[VY]*v.n[VY] + a->v[2].n[VZ]*v.n[VZ];

    return av;
}

c3vec3 c3vec3_mulm3(const c3vec3 v, const c3mat3p a)
{
	c3mat3 t = c3mat3_transpose(a);
    return c3mat3_mulv3(&t, v);
}

c3f c3vec2_dot(const c3vec2 a, const c3vec2 b)
{
    return a.n[VX]*b.n[VX] + a.n[VY]*b.n[VY];
}

c3vec3 c3vec2_cross(const c3vec2 a, const c3vec2 b)
{
    return c3vec3f(0.0, 0.0, a.n[VX] * b.n[VY] - b.n[VX] * a.n[VY]);
}

int c3vec2_equal(const c3vec2 a, const c3vec2 b)
{
    return (a.n[VX] == b.n[VX]) && (a.n[VY] == b.n[VY]);
}


c3vec2 c3vec2_min(const c3vec2 a, const c3vec2 b)
{
    return c3vec2f(MIN(a.n[VX], b.n[VX]), MIN(a.n[VY], b.n[VY]));
}

c3vec2 c3vec2_max(const c3vec2 a, const c3vec2 b)
{
    return c3vec2f(MAX(a.n[VX], b.n[VX]), MAX(a.n[VY], b.n[VY]));
}

c3vec2 c3vec2_prod(const c3vec2 a, const c3vec2 b)
{
    return c3vec2f(a.n[VX] * b.n[VX], a.n[VY] * b.n[VY]);
}

/****************************************************************
 *                                                              *
 *          c3vec3 Member functions                               *
 *                                                              *
 ****************************************************************/

// CONSTRUCTORS

c3vec3 c3vec3_zero()
{
	c3vec3 n;// = { .x = 0, .y = 0, .z = 0 };// older gcc <4.6 doesn't like this
	n.x = n.y = n.z = 0;
    return n;
}

c3vec3 c3vec3f(c3f x, c3f y, c3f z)
{
	c3vec3 v;// = { .x = x, .y = y, .z = z };// older gcc <4.6 doesn't like this
	v.x = x; v.y = y; v.z = z;
	return v;
}

c3vec3 c3vec3_vec2f(const c3vec2 v, c3f d)
{
	c3vec3 n;// = { .x = v.x, .y = v.y, .z = d }; // older gcc <4.6 doesn't like this
	n.x = v.x; n.y = v.y; n.z = d;
	return n;
}

c3vec3 c3vec3_vec2(const c3vec2 v)
{
	return c3vec3_vec2f(v, 1.0);
}

c3vec3 c3vec3_vec4(const c3vec4 v) // it is up to caller to avoid divide-by-zero
{
	c3vec3 n;
    n.n[VX] = v.n[VX] / v.n[VW];
    n.n[VY] = v.n[VY] / v.n[VW];
    n.n[VZ] = v.n[VZ] / v.n[VW];
    return n;
}


c3vec3 c3vec3_add(c3vec3 a, const c3vec3 v)
{
    a.n[VX] += v.n[VX];
    a.n[VY] += v.n[VY];
    a.n[VZ] += v.n[VZ];
    return a;
}

c3vec3 c3vec3_sub(c3vec3 a, const c3vec3 v)
{
	a.n[VX] -= v.n[VX];
	a.n[VY] -= v.n[VY];
	a.n[VZ] -= v.n[VZ];
    return a;
}

c3vec3 c3vec3_mulf(c3vec3 a, c3f d)
{
	a.n[VX] *= d;
	a.n[VY] *= d;
	a.n[VZ] *= d;
    return a;
}

c3vec3 c3vec3_divf(c3vec3 a, c3f d)
{
    c3f d_inv = 1.0f/d;
    a.n[VX] *= d_inv;
    a.n[VY] *= d_inv;
    a.n[VZ] *= d_inv;
    return a;
}

// SPECIAL FUNCTIONS

c3f c3vec3_length2(const c3vec3 a)
{
    return a.n[VX]*a.n[VX] + a.n[VY]*a.n[VY] + a.n[VZ]*a.n[VZ];
}

c3f c3vec3_length(const c3vec3 a)
{
    return (c3f) sqrt(c3vec3_length2(a));
}

c3vec3 c3vec3_normalize(const c3vec3 a) // it is up to caller to avoid divide-by-zero
{
    return c3vec3_divf(a, c3vec3_length(a));
}

c3vec3 c3vec3_homogenize(c3vec3 a) // it is up to caller to avoid divide-by-zero
{
    a.n[VX] /= a.n[VZ];
    a.n[VY] /= a.n[VZ];
    a.n[VZ] = 1.0;
    return a;
}

c3vec3 c3vec3_apply(c3vec3 a, V_FCT_PTR fct)
{
    a.n[VX] = fct(a.n[VX]);
    a.n[VY] = fct(a.n[VY]);
    a.n[VZ] = fct(a.n[VZ]);
    return a;
}

// FRIENDS

c3vec3 c3vec3_minus(const c3vec3 a)
{
    return c3vec3f(-a.n[VX],-a.n[VY],-a.n[VZ]);
}

#if later
c3vec3 operator*(const c3mat4 &a, const c3vec3 &v)
{
    return a*c3vec4(v);
}

c3vec3 operator*(const c3vec3 &v, c3mat4 &a)
{
    return a.transpose()*v;
}
#endif

c3f c3vec3_dot(const c3vec3 a, const c3vec3 b)
{
    return a.n[VX]*b.n[VX] + a.n[VY]*b.n[VY] + a.n[VZ]*b.n[VZ];
}

c3vec3 c3vec3_cross(const c3vec3 a, const c3vec3 b)
{
    return
        c3vec3f(a.n[VY]*b.n[VZ] - a.n[VZ]*b.n[VY],
             a.n[VZ]*b.n[VX] - a.n[VX]*b.n[VZ],
             a.n[VX]*b.n[VY] - a.n[VY]*b.n[VX]);
}

int c3vec3_equal(const c3vec3 a, const c3vec3 b)
{
    return (a.n[VX] == b.n[VX]) && (a.n[VY] == b.n[VY]) && (a.n[VZ] == b.n[VZ]);
}


c3vec3 c3vec3_min(const c3vec3 a, const c3vec3 b)
{
    return c3vec3f(
        MIN(a.n[VX], b.n[VX]),
        MIN(a.n[VY], b.n[VY]),
        MIN(a.n[VZ], b.n[VZ]));
}

c3vec3 c3vec3_max(const c3vec3 a, const c3vec3 b)
{
    return c3vec3f(
        MAX(a.n[VX], b.n[VX]),
        MAX(a.n[VY], b.n[VY]),
        MAX(a.n[VZ], b.n[VZ]));
}

c3vec3 c3vec3_prod(const c3vec3 a, const c3vec3 b)
{
    return c3vec3f(a.n[VX]*b.n[VX], a.n[VY]*b.n[VY], a.n[VZ]*b.n[VZ]);
}

c3vec3 c3vec3_polar(const c3vec3 a)
{
	c3f l = c3vec3_length(a);
	c3f phi = atan2(a.y, a.x);
	c3f theta = acos(a.z / l);
	return c3vec3f(phi, theta, l);
}

/****************************************************************
 *                                                              *
 *          c3vec4 Member functions                               *
 *                                                              *
 ****************************************************************/

// CONSTRUCTORS

c3vec4 c3vec4_zero()
{
	c3vec4 n ;//= { .x = 0, .y = 0, .z = 0, .w = 1.0 }; // older gcc <4.6 doesn't like this
	n.x = n.y = n.z = 0; n.w = 1.0;
    return n;
}

c3vec4 c3vec4f(c3f x, c3f y, c3f z, c3f w)
{
	c3vec4 n;// = { .x = x, .y = y, .z = z, .w = w }; // older gcc <4.6 doesn't like this
	n.x =x;  n.y = y; n.z = z; n.w = w;
    return n;
}

c3vec4 c3vec4_vec3(const c3vec3 v)
{
	return c3vec4f(v.n[VX], v.n[VY], v.n[VZ], 1.0);
}

c3vec4 c3vec4_vec3f(const c3vec3 v, c3f d)
{
	return c3vec4f(v.n[VX], v.n[VY], v.n[VZ], d);
}

// ASSIGNMENT OPERATORS

c3vec4 c3vec4_add(c3vec4 a, const c3vec4 v)
{
    a.n[VX] += v.n[VX];
    a.n[VY] += v.n[VY];
    a.n[VZ] += v.n[VZ];
    a.n[VW] += v.n[VW];
    return a;
}

c3vec4 c3vec4_sub(c3vec4 a, const c3vec4 v)
{
	a.n[VX] -= v.n[VX];
	a.n[VY] -= v.n[VY];
	a.n[VZ] -= v.n[VZ];
	a.n[VW] -= v.n[VW];
    return a;
}

c3vec4 c3vec4_mulf(c3vec4 a, c3f d)
{
    a.n[VX] *= d;
    a.n[VY] *= d;
    a.n[VZ] *= d;
    a.n[VW] *= d;
    return a;
}

c3vec4 c3vec4_divf(c3vec4 a, c3f d)
{
    c3f d_inv = 1.0f/d;
    a.n[VX] *= d_inv;
    a.n[VY] *= d_inv;
    a.n[VZ] *= d_inv;
    a.n[VW] *= d_inv;
    return a;
}

// SPECIAL FUNCTIONS

c3f c3vec4_length2(const c3vec4 a)
{
    return a.n[VX]*a.n[VX] + a.n[VY]*a.n[VY] + a.n[VZ]*a.n[VZ] + a.n[VW]*a.n[VW];
}

c3f c3vec4_length(const c3vec4 a)
{
    return (c3f) sqrt(c3vec4_length2(a));
}

c3vec4 c3vec4_normalize(c3vec4 a) // it is up to caller to avoid divide-by-zero
{
    return c3vec4_divf(a, c3vec4_length(a));
}

c3vec4 c3vec4_homogenize(c3vec4 a) // it is up to caller to avoid divide-by-zero
{
    a.n[VX] /= a.n[VW];
    a.n[VY] /= a.n[VW];
    a.n[VZ] /= a.n[VW];
    a.n[VW] = 1.0;
    return a;
}

c3vec4 c3vec4_apply(c3vec4 a, V_FCT_PTR fct)
{
    a.n[VX] = fct(a.n[VX]);
    a.n[VY] = fct(a.n[VY]);
    a.n[VZ] = fct(a.n[VZ]);
    a.n[VW] = fct(a.n[VW]);
    return a;
}

c3vec4 c3mat4_mulv4(const c3mat4p a, const c3vec4 v)
{
    #define ROWCOL(i) \
        a->v[i].n[0]*v.n[VX] + \
        a->v[i].n[1]*v.n[VY] + \
        a->v[i].n[2]*v.n[VZ] + \
        a->v[i].n[3]*v.n[VW]

    return c3vec4f(ROWCOL(0), ROWCOL(1), ROWCOL(2), ROWCOL(3));

    #undef ROWCOL
}

c3vec4 c3vec4_mulm4(const c3vec4 v, const c3mat4p a)
{
	c3mat4 m = c3mat4_transpose(a);
    return c3mat4_mulv4(&m, v);
}

c3vec3 c3mat4_mulv3(const c3mat4p a, const c3vec3 v)
{
	return c3vec3_vec4(c3mat4_mulv4(a, c3vec4_vec3(v)));
}

c3vec4 c3vec4_minus(const c3vec4 a)
{
    return c3vec4f(-a.n[VX],-a.n[VY],-a.n[VZ],-a.n[VW]);
}

int c3vec4_equal(const c3vec4 a, const c3vec4 b)
{
    return
        (a.n[VX] == b.n[VX]) &&
        (a.n[VY] == b.n[VY]) &&
        (a.n[VZ] == b.n[VZ]) &&
        (a.n[VW] == b.n[VW]);
}

c3vec4 c3vec4_min(const c3vec4 a, const c3vec4 b)
{
    return c3vec4f(
        MIN(a.n[VX], b.n[VX]),
        MIN(a.n[VY], b.n[VY]),
        MIN(a.n[VZ], b.n[VZ]),
        MIN(a.n[VW], b.n[VW]));
}

c3vec4 c3vec4_max(const c3vec4 a, const c3vec4 b)
{
    return c3vec4f(
        MAX(a.n[VX], b.n[VX]),
        MAX(a.n[VY], b.n[VY]),
        MAX(a.n[VZ], b.n[VZ]),
        MAX(a.n[VW], b.n[VW]));
}

c3vec4 c3vec4_prod(const c3vec4 a, const c3vec4 b)
{
    return c3vec4f(
        a.n[VX] * b.n[VX],
        a.n[VY] * b.n[VY],
        a.n[VZ] * b.n[VZ],
        a.n[VW] * b.n[VW]);
}

/****************************************************************
 *                                                              *
 *          c3mat3 member functions                               *
 *                                                              *
 ****************************************************************/

// CONSTRUCTORS

c3mat3 c3mat3_identity()
{
	return identity2D();
}

c3mat3 c3mat3_vec3(const c3vec3 v0, const c3vec3 v1, const c3vec3 v2)
{
	c3mat3 m = { .v[0] = v0, .v[1] = v1, .v[2] = v2 };
	return m;
}

c3mat3p c3mat3_add(const c3mat3p a, const c3mat3p m)
{
	a->v[0] = c3vec3_add(a->v[0], m->v[0]);
	a->v[1] = c3vec3_add(a->v[1], m->v[1]);
	a->v[2] = c3vec3_add(a->v[2], m->v[2]);
    return a;
}

c3mat3p c3mat3_sub(const c3mat3p a, const c3mat3p m)
{
	a->v[0] = c3vec3_sub(a->v[0], m->v[0]);
	a->v[1] = c3vec3_sub(a->v[1], m->v[1]);
	a->v[2] = c3vec3_sub(a->v[2], m->v[2]);
    return a;
}

c3mat3p c3mat3_mulf(const c3mat3p a, c3f d)
{
	a->v[0] = c3vec3_mulf(a->v[0], d);
	a->v[1] = c3vec3_mulf(a->v[1], d);
	a->v[2] = c3vec3_mulf(a->v[2], d);
    return a;
}

c3mat3p c3mat3_divf(const c3mat3p a, c3f d)
{
	a->v[0] = c3vec3_divf(a->v[0], d);
	a->v[1] = c3vec3_divf(a->v[1], d);
	a->v[2] = c3vec3_divf(a->v[2], d);
    return a;
}

// SPECIAL FUNCTIONS

c3mat3 c3mat3_transpose(const c3mat3p a)
{
    return c3mat3_vec3(
        c3vec3f(a->v[0].n[0], a->v[1].n[0], a->v[2].n[0]),
        c3vec3f(a->v[0].n[1], a->v[1].n[1], a->v[2].n[1]),
        c3vec3f(a->v[0].n[2], a->v[1].n[2], a->v[2].n[2]));
}

c3mat3 c3mat3_inverse(const c3mat3p m)   // Gauss-Jordan elimination with partial pivoting
{
	c3mat3 a = *m; // As a evolves from original mat into identity
	c3mat3 b = c3mat3_identity(); // b evolves from identity into inverse(a)
	int i, j, i1;

	// Loop over cols of a from left to right, eliminating above and below diag
	for (j = 0; j < 3; j++) { // Find largest pivot in column j among rows j..2
		i1 = j; // Row with largest pivot candidate
		for (i = j + 1; i < 3; i++)
			if (fabs(a.v[i].n[j]) > fabs(a.v[i1].n[j]))
				i1 = i;

		// Swap rows i1 and j in a and b to put pivot on diagonal
		c3vec3 _s;
		_s = a.v[i1]; a.v[i1] = a.v[j]; a.v[j] = _s;  // swap(a.v[i1], a.v[j]);
		_s = b.v[i1]; b.v[i1] = b.v[j]; b.v[j] = _s;  //swap(b.v[i1], b.v[j]);

		// Scale row j to have a unit diagonal
		if (a.v[j].n[j] == 0.) {
		//	VEC_ERROR("c3mat3::inverse: singular matrix; can't invert\n");
			return *m;
		}

		b.v[j] = c3vec3_divf(b.v[j], a.v[j].n[j]);
		a.v[j] = c3vec3_divf(a.v[j], a.v[j].n[j]);

		// Eliminate off-diagonal elems in col j of a, doing identical ops to b
		for (i = 0; i < 3; i++)
			if (i != j) {
				b.v[i] = c3vec3_sub(b.v[i], c3vec3_mulf(b.v[j], a.v[i].n[j]));
				a.v[i] = c3vec3_sub(a.v[i], c3vec3_mulf(a.v[j], a.v[i].n[j]));
			}
	}

	return b;
}

c3mat3p c3mat3_apply(c3mat3p a, V_FCT_PTR fct)
{
	a->v[0] = c3vec3_apply(a->v[0], fct);
	a->v[1] = c3vec3_apply(a->v[1], fct);
	a->v[2] = c3vec3_apply(a->v[2], fct);
    return a;
}


c3mat3 c3mat3_minus(const c3mat3p a)
{
    return c3mat3_vec3(
    		c3vec3_minus(a->v[0]),
    		c3vec3_minus(a->v[1]),
    		c3vec3_minus(a->v[2]));
}

c3mat3 c3mat3_mul(const c3mat3p a, const c3mat3p b)
{
    #define ROWCOL(i, j) \
    a->v[i].n[0]*b->v[0].n[j] + a->v[i].n[1]*b->v[1].n[j] + a->v[i].n[2]*b->v[2].n[j]

    return c3mat3_vec3(
        c3vec3f(ROWCOL(0,0), ROWCOL(0,1), ROWCOL(0,2)),
        c3vec3f(ROWCOL(1,0), ROWCOL(1,1), ROWCOL(1,2)),
        c3vec3f(ROWCOL(2,0), ROWCOL(2,1), ROWCOL(2,2)));

    #undef ROWCOL
}

int c3mat3_equal(const c3mat3p a, const c3mat3p b)
{
    return
        c3vec3_equal(a->v[0], b->v[0]) &&
        c3vec3_equal(a->v[1], b->v[1]) &&
        c3vec3_equal(a->v[2], b->v[2]);
}

/****************************************************************
 *                                                              *
 *          c3mat4 member functions                               *
 *                                                              *
 ****************************************************************/

// CONSTRUCTORS

c3mat4 c3mat4_identity()
{
    return identity3D();
}

c3mat4 c3mat4_vec4(const c3vec4 v0, const c3vec4 v1, const c3vec4 v2, const c3vec4 v3)
{
	c3mat4 m = { .v[0] = v0, .v[1] = v1, .v[2] = v2, .v[3] = v3 };
	return m;
}

c3mat4 c3mat4f(
     c3f a00, c3f a01, c3f a02, c3f a03,
     c3f a10, c3f a11, c3f a12, c3f a13,
     c3f a20, c3f a21, c3f a22, c3f a23,
     c3f a30, c3f a31, c3f a32, c3f a33 )
{
	c3mat4 m;
	m.v[0] = c3vec4f(a00, a01, a01, a03);
	m.v[1] = c3vec4f(a10, a11, a11, a13);
	m.v[2] = c3vec4f(a20, a21, a21, a23);
	m.v[3] = c3vec4f(a30, a31, a21, a33);
	return m;
}

c3mat4p c3mat4p_add(c3mat4p a, const c3mat4p m)
{
    a->v[0] = c3vec4_add(a->v[0], m->v[0]);
    a->v[1] = c3vec4_add(a->v[1], m->v[1]);
    a->v[2] = c3vec4_add(a->v[2], m->v[2]);
    a->v[3] = c3vec4_add(a->v[3], m->v[3]);
    return a;
}

c3mat4p c3mat4p_sub(c3mat4p a, const c3mat4p m)
{
    a->v[0] = c3vec4_sub(a->v[0], m->v[0]);
    a->v[1] = c3vec4_sub(a->v[1], m->v[1]);
    a->v[2] = c3vec4_sub(a->v[2], m->v[2]);
    a->v[3] = c3vec4_sub(a->v[3], m->v[3]);
    return a;
}

c3mat4p c3mat4p_mulf(c3mat4p a, c3f d)
{
    a->v[0] = c3vec4_mulf(a->v[0], d);
    a->v[1] = c3vec4_mulf(a->v[1], d);
    a->v[2] = c3vec4_mulf(a->v[2], d);
    a->v[3] = c3vec4_mulf(a->v[3], d);
    return a;
}

c3mat4p c3mat4p_divf(c3mat4p a, c3f d)
{
    a->v[0] = c3vec4_divf(a->v[0], d);
    a->v[1] = c3vec4_divf(a->v[1], d);
    a->v[2] = c3vec4_divf(a->v[2], d);
    a->v[3] = c3vec4_divf(a->v[3], d);
    return a;
}

// SPECIAL FUNCTIONS;

c3mat4 c3mat4_transpose(const c3mat4p a)
{
    return c3mat4_vec4(
        c3vec4f(a->v[0].n[0], a->v[1].n[0], a->v[2].n[0], a->v[3].n[0]),
        c3vec4f(a->v[0].n[1], a->v[1].n[1], a->v[2].n[1], a->v[3].n[1]),
        c3vec4f(a->v[0].n[2], a->v[1].n[2], a->v[2].n[2], a->v[3].n[2]),
        c3vec4f(a->v[0].n[3], a->v[1].n[3], a->v[2].n[3], a->v[3].n[3]));
}

c3mat4 c3mat4_inverse(const c3mat4p m)       // Gauss-Jordan elimination with partial pivoting
{
	c3mat4 a = *m; // As a evolves from original mat into identity
	c3mat4 b = identity3D(); // b evolves from identity into inverse(a)
	int i, j, i1;

	// Loop over cols of a from left to right, eliminating above and below diag
	for (j = 0; j < 4; j++) { // Find largest pivot in column j among rows j..3
		i1 = j; // Row with largest pivot candidate
		for (i = j + 1; i < 4; i++)
			if (fabs(a.v[i].n[j]) > fabs(a.v[i1].n[j]))
				i1 = i;

		// Swap rows i1 and j in a and b to put pivot on diagonal
		c3vec4 _s;
		_s = a.v[i1]; a.v[i1] = a.v[j]; a.v[j] = _s; // swap(a.v[i1], a.v[j]);
		_s = b.v[i1]; b.v[i1] = b.v[j]; b.v[j] = _s; // swap(b.v[i1], b.v[j]);

		// Scale row j to have a unit diagonal
		if (a.v[j].n[j] == 0.) {
			//    VEC_ERROR("c3mat4::inverse: singular matrix; can't invert\n");
			return a;
		}
		b.v[j] = c3vec4_divf(b.v[j], a.v[j].n[j]);
		a.v[j] = c3vec4_divf(a.v[j], a.v[j].n[j]);

		// Eliminate off-diagonal elems in col j of a, doing identical ops to b
		for (i = 0; i < 4; i++)
			if (i != j) {
				b.v[i] = c3vec4_sub(b.v[i], c3vec4_mulf(b.v[j], a.v[i].n[j]));
				a.v[i] = c3vec4_sub(a.v[i], c3vec4_mulf(a.v[j], a.v[i].n[j]));
			}
	}

	return b;
}

c3mat4p c3mat4p_apply(c3mat4p a, V_FCT_PTR fct)
{
	a->v[0] = c3vec4_apply(a->v[0], fct);
	a->v[1] = c3vec4_apply(a->v[1], fct);
	a->v[2] = c3vec4_apply(a->v[2], fct);
	a->v[3] = c3vec4_apply(a->v[3], fct);
    return a;
}

c3mat4p c3mat4p_swap_rows(c3mat4p a, int i, int j)
{
    c3vec4 t;

    t    = a->v[i];
    a->v[i] = a->v[j];
    a->v[j] = t;
    return a;
}

c3mat4p c3mat4p_swap_cols(c3mat4p a, int i, int j)
{
	c3f t;

	for (int k = 0; k < 4; k++) {
		t = a->v[k].n[i];
		a->v[k].n[i] = a->v[k].n[j];
		a->v[k].n[j] = t;
	}
	return a;
}


// FRIENDS

c3mat4 c3mat4_minus(const c3mat4p a)
{
    return c3mat4_vec4(
    		c3vec4_minus(a->v[0]),
    		c3vec4_minus(a->v[1]),
    		c3vec4_minus(a->v[2]),
    		c3vec4_minus(a->v[3]));
}

c3mat4 c3mat4_add(const c3mat4p a, const c3mat4p b)
{
    return c3mat4_vec4(
        c3vec4_add(a->v[0], b->v[0]),
        c3vec4_add(a->v[1], b->v[1]),
        c3vec4_add(a->v[2], b->v[2]),
        c3vec4_add(a->v[3], b->v[3]));
}

c3mat4 c3mat4_sub(const c3mat4p a, const c3mat4p b)
{
    return c3mat4_vec4(
        c3vec4_sub(a->v[0], b->v[0]),
        c3vec4_sub(a->v[1], b->v[1]),
        c3vec4_sub(a->v[2], b->v[2]),
        c3vec4_sub(a->v[3], b->v[3]));
}

c3mat4 c3mat4_mul(const c3mat4p a, const c3mat4p b)
{
    #define ROWCOL(i, j) \
        a->v[i].n[0]*b->v[0].n[j] + \
        a->v[i].n[1]*b->v[1].n[j] + \
        a->v[i].n[2]*b->v[2].n[j] + \
        a->v[i].n[3]*b->v[3].n[j]

    return c3mat4_vec4(
        c3vec4f(ROWCOL(0,0), ROWCOL(0,1), ROWCOL(0,2), ROWCOL(0,3)),
        c3vec4f(ROWCOL(1,0), ROWCOL(1,1), ROWCOL(1,2), ROWCOL(1,3)),
        c3vec4f(ROWCOL(2,0), ROWCOL(2,1), ROWCOL(2,2), ROWCOL(2,3)),
        c3vec4f(ROWCOL(3,0), ROWCOL(3,1), ROWCOL(3,2), ROWCOL(3,3))
        );

    #undef ROWCOL
}

c3mat4 c3mat4_mulf(const c3mat4p a, c3f d)
{
	c3mat4 r = *a;
	return *c3mat4p_mulf(&r, d);
}

c3mat4 c3mat4_divf(const c3mat4p a, c3f d)
{
	c3mat4 r = *a;
	return *c3mat4p_divf(&r, d);
}

int c3mat4_equal(const c3mat4p a, const c3mat4p b)
{
	return !memcmp(a->n, b->n, sizeof(a->n));
#if 0
    return
        c3vec4_equal(a->v[0], b->v[0]) &&
        c3vec4_equal(a->v[1], b->v[1]) &&
        c3vec4_equal(a->v[2], b->v[2]) &&
        c3vec4_equal(a->v[3], b->v[3]);
#endif
}

/****************************************************************
 *                                                              *
 *         2D functions and 3D functions                        *
 *                                                              *
 ****************************************************************/

c3mat3 identity2D()
{
    return c3mat3_vec3(
        c3vec3f(1.0, 0.0, 0.0),
        c3vec3f(0.0, 1.0, 0.0),
        c3vec3f(0.0, 0.0, 1.0));
}

c3mat3 translation2D(const c3vec2 v)
{
    return c3mat3_vec3(
        c3vec3f(1.0, 0.0, v.n[VX]),
        c3vec3f(0.0, 1.0, v.n[VY]),
        c3vec3f(0.0, 0.0, 1.0));
}

c3mat3 rotation2D(const c3vec2 Center, c3f angleDeg)
{
    c3f angleRad = (c3f) (angleDeg * PI_OVER_180);
    c3f c = (c3f) cos(angleRad);
    c3f s = (c3f) sin(angleRad);

    return c3mat3_vec3(
        c3vec3f(c,    -s, Center.n[VX] * (1.0f-c) + Center.n[VY] * s),
        c3vec3f(s,     c, Center.n[VY] * (1.0f-c) - Center.n[VX] * s),
        c3vec3f(0.0, 0.0, 1.0));
}

c3mat3 scaling2D(const c3vec2 scaleVector)
{
    return c3mat3_vec3(
        c3vec3f(scaleVector.n[VX], 0.0, 0.0),
        c3vec3f(0.0, scaleVector.n[VY], 0.0),
        c3vec3f(0.0, 0.0, 1.0));
}

c3mat4
identity3D()
{
    return c3mat4_vec4(
        c3vec4f(1.0, 0.0, 0.0, 0.0),
        c3vec4f(0.0, 1.0, 0.0, 0.0),
        c3vec4f(0.0, 0.0, 1.0, 0.0),
        c3vec4f(0.0, 0.0, 0.0, 1.0));
}

c3mat4 translation3D(const c3vec3 v)
{
    return c3mat4_vec4(
        c3vec4f(1.0, 0.0, 0.0, 0.0),
        c3vec4f(0.0, 1.0, 0.0, 0.0),
        c3vec4f(0.0, 0.0, 1.0, 0.0),
        c3vec4f(v.n[VX], v.n[VY], v.n[VZ], 1.0));
}

c3mat4 rotation3D(const c3vec3 Axis, c3f angleDeg)
{
    c3f angleRad = (c3f) (angleDeg * PI_OVER_180);
    c3f c = (c3f) cos(angleRad);
    c3f s = (c3f) sin(angleRad);
    c3f t = 1.0f - c;

    c3vec3 axis = c3vec3_normalize(Axis);

    return c3mat4_vec4(
        c3vec4f(
        	t * axis.n[VX] * axis.n[VX] + c,
        	t * axis.n[VX] * axis.n[VY] - s * axis.n[VZ],
			t * axis.n[VX] * axis.n[VZ] + s * axis.n[VY],
			0.0),
        c3vec4f(
			t * axis.n[VX] * axis.n[VY] + s * axis.n[VZ],
			t * axis.n[VY] * axis.n[VY] + c,
			t * axis.n[VY] * axis.n[VZ] - s * axis.n[VX],
			0.0),
        c3vec4f(
			t * axis.n[VX] * axis.n[VZ] - s * axis.n[VY],
			t * axis.n[VY] * axis.n[VZ] + s * axis.n[VX],
			t * axis.n[VZ] * axis.n[VZ] + c,
			0.0),
        c3vec4f(0.0, 0.0, 0.0, 1.0));
}

c3mat4 rotation3Drad(const c3vec3 Axis, c3f angleRad)
{
    c3f c = (c3f) cos(angleRad);
    c3f s = (c3f) sin(angleRad);
    c3f t = 1.0f - c;

    c3vec3 axis = c3vec3_normalize(Axis);

    return c3mat4_vec4(
        c3vec4f(t * axis.n[VX] * axis.n[VX] + c,
             t * axis.n[VX] * axis.n[VY] - s * axis.n[VZ],
             t * axis.n[VX] * axis.n[VZ] + s * axis.n[VY],
             0.0),
        c3vec4f(t * axis.n[VX] * axis.n[VY] + s * axis.n[VZ],
             t * axis.n[VY] * axis.n[VY] + c,
             t * axis.n[VY] * axis.n[VZ] - s * axis.n[VX],
             0.0),
        c3vec4f(t * axis.n[VX] * axis.n[VZ] - s * axis.n[VY],
             t * axis.n[VY] * axis.n[VZ] + s * axis.n[VX],
             t * axis.n[VZ] * axis.n[VZ] + c,
             0.0),
        c3vec4f(0.0, 0.0, 0.0, 1.0));
}

c3mat4 scaling3D(const c3vec3 scaleVector)
{
    return c3mat4_vec4(
        c3vec4f(scaleVector.n[VX], 0.0, 0.0, 0.0),
        c3vec4f(0.0, scaleVector.n[VY], 0.0, 0.0),
        c3vec4f(0.0, 0.0, scaleVector.n[VZ], 0.0),
        c3vec4f(0.0, 0.0, 0.0, 1.0));
}

c3mat4	frustum3D(
			c3f left, c3f right, c3f bottom, c3f top,
			c3f znear, c3f zfar)
{
    c3f temp = 2.0 * znear,
    		temp2 = right - left,
    		temp3 = top - bottom,
    		temp4 = zfar - znear;
    return c3mat4_vec4(
  		  c3vec4f(temp / temp2, 0.0, 0.0, 0.0),
		  c3vec4f(0.0, temp / temp3, 0.0, 0.0),
		  c3vec4f((right + left) / temp2, (top + bottom) / temp3, (-zfar - znear) / temp4, -1.0),
		  c3vec4f(-1.0, 0.0, (-temp * zfar) / temp4, 0.0));
}

c3mat4	perspective3D(c3f fov, c3f aspect, c3f zNear, c3f zFar)
{
	c3f ymax = zNear * tan(fov * PI_OVER_360);
	c3f ymin = -ymax;
	c3f xmin = ymin * aspect;
	c3f xmax = ymax * aspect;

	return frustum3D(xmin, xmax, ymin, ymax, zNear, zFar);
}
