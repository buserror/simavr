/*
	c3quaternion.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include "c3quaternion.h"

#ifndef DEG2RAD
#define DEG2RAD(x) ((x)/180.0*M_PI)
#define RAD2DEG(x) ((x)/M_PI*180.0)
#endif
#ifndef FUDGE
#define FUDGE .00001
#endif

c3quat
c3quat_new()
{
    return c3quat_identity();
}

/************************************************* c3quat_identity() *****/
/* Returns quaternion identity element                                 */

c3quat
c3quat_identity()
{
    return c3quat_vec3( c3vec3f( 0.0, 0.0, 0.0 ), 1.0 );
}

c3quat
c3quatf(
		const c3f x,
		const c3f y,
		const c3f z,
		const c3f w)
{
	c3quat q = { .v = c3vec3f(x,y,z), .s = w };
	return q;
}

c3quat
c3quat_vec3(
		const c3vec3 v,
		const c3f s)
{
	c3quat q = { .v = v, .s = s };
    return q;
}

c3quat
c3quat_vec4(
		const c3vec4 v)
{
	c3quat q = { .v = c3vec3f(v.n[0], v.n[1], v.n[2]), .s = v.n[3] };
    return q;
}

c3quat
c3quat_double(
		const double *d)
{
	c3quat q;
    q.v.n[0] = (c3f) d[0];
    q.v.n[1] = (c3f) d[1];
    q.v.n[2] = (c3f) d[2];
    q.s    	 = (c3f) d[3];
    return q;
}


c3quat
c3quat_add(
		const c3quat a,
		const c3quat b)
{
    return c3quat_vec3(c3vec3_add(a.v, b.v), a.s + b.s );
}

c3quat
c3quat_sub(
		const c3quat a,
		const c3quat b)
{
    return c3quat_vec3(c3vec3_sub(a.v, b.v), a.s - b.s );
}

c3quat
c3quat_minus(
		const c3quat a )
{
    return c3quat_vec3(c3vec3_minus(a.v), -a.s);
}

c3quat
c3quat_mul(
		const c3quat a,
		const c3quat b)
{
//    return c3quat( a.s*b.s - a.v*b.v, a.s*b.v + b.s*a.v + a.v^b.v );
    return c3quat_vec3(
    		c3vec3_add(c3vec3_mulf(b.v, a.s), c3vec3_add(c3vec3_mulf(a.v, b.s), c3vec3_cross(a.v, b.v))),
    		(a.s * b.s) - c3vec3_dot(a.v, b.v));
}

c3quat
c3quat_mulf( const c3quat a, const c3f t)
{
    return c3quat_vec3(c3vec3_mulf(a.v, t), a.s * t );
}

c3mat4
c3quat_to_mat4(
		const c3quat a )
{
    c3f xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

    c3f t  = 2.0f / (c3vec3_dot(a.v, a.v) + (a.s * a.s));

    xs = a.v.n[VX]*t;   ys = a.v.n[VY]*t;   zs = a.v.n[VZ]*t;
    wx = a.s*xs;		wy = a.s*ys;		wz = a.s*zs;
    xx = a.v.n[VX]*xs;  xy = a.v.n[VX]*ys;  xz = a.v.n[VX]*zs;
    yy = a.v.n[VY]*ys;  yz = a.v.n[VY]*zs;  zz = a.v.n[VZ]*zs;

    c3mat4 m = c3mat4_vec4(
           c3vec4f(1.0f-(yy+zz), xy+wz,        xz-wy,        0.0f),
           c3vec4f(xy-wz,        1.0f-(xx+zz), yz+wx,        0.0f),
           c3vec4f(xz+wy,        yz-wx,        1.0f-(xx+yy), 0.0f),
           c3vec4f(0.0f,         0.0f,         0.0f,         1.0f ));

    return m;
}


/************************************************ quat_slerp() ********/
/* Quaternion spherical interpolation                                 */

c3quat
quat_slerp(
		const c3quat from,
		const c3quat to,
		c3f t)
{
    c3quat to1;
    c3f omega, cosom, sinom, scale0, scale1;

    /* calculate cosine */
    cosom = c3vec3_dot(from.v, to.v) + from.s + to.s;

	/* Adjust signs (if necessary) */
	if (cosom < 0.0) {
		cosom = -cosom;
		to1 = c3quat_minus(to);
	} else {
		to1 = to;
	}

    /* Calculate coefficients */
    if ((1.0 - cosom) > FUDGE ) {
        /* standard case (slerp) */
        omega =  (c3f) acos( cosom );
        sinom =  (c3f) sin( omega );
        scale0 = (c3f) sin((1.0 - t) * omega) / sinom;
        scale1 = (c3f) sin(t * omega) / sinom;
    } else {
        /* 'from' and 'to' are very close - just do linear interpolation */
        scale0 = 1.0f - t;
        scale1 = t;
    }

    return c3quat_add(c3quat_mulf(from, scale0), c3quat_mulf(to1, scale1));
}

/********************************************** set_angle() ************/
/* set rot angle (degrees)                                             */

c3quatp
c3quat_set_angle(
		c3quatp a,
		c3f f)
{
    c3vec3 axis = c3quat_get_axis(a);

    a->s = (c3f) cos( DEG2RAD( f ) / 2.0 );

    a->v = c3vec3_mulf(axis, (c3f) sin(DEG2RAD(f) / 2.0));
    return a;
}

/********************************************** scale_angle() ************/
/* scale rot angle (degrees)                                             */

c3quatp
c3quat_scale_angle(
		c3quatp a,
		c3f f)
{
	return c3quat_set_angle(a, f * c3quat_get_angle(a) );
}

/********************************************** get_angle() ************/
/* get rot angle (degrees).  Assumes s is between -1 and 1             */

c3f
c3quat_get_angle(
		const c3quatp a)
{
    return (c3f) RAD2DEG( 2.0 * acos( a->s ) );
}

/********************************************* get_axis() **************/

c3vec3
c3quat_get_axis(
		c3quatp a)
{
    c3f scale = (c3f) sin( acos( a->s ) );

    if ( scale < FUDGE && scale > -FUDGE )
        return c3vec3f( 0.0, 0.0, 0.0 );
    else
        return  c3vec3_divf(a->v, scale);
}

/******************************************* c3quat_print() ************/
#if 0
void c3quat_print(FILE *dest, const char *name) const
{
    fprintf( dest, "%s:   v:<%3.2f %3.2f %3.2f>  s:%3.2f\n",
        name, v[0], v[1], v[2], s );
}
#endif
