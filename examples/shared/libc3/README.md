libc3 - No frill 'scene' graph library in C
=====
(C) 2012 Michel Pollet <buserror@gmail.com>

**WARNING** This API is not your nanny. It is made to be lean, mean, efficient
with no frill, no asserts, no bounds checking, no sugar coating.

On the other hand it's fast, reasonably clean and is a micro-fraction of the
other giganormous 'scene graphs' or 'game engine' libraries around.

It's vaguely inspired by THREE.js funnily enough, because it allows you to
hack around and quickly get stuff on screen with the minimal amount of 
effort.

The API has various bits:
* c3algebra: C derivative of an old C++ piece of code I had lying around and that has
been present in my toolset for a long time. It gives you *vectors* (c3vec2, c3vec3, c3vec4)
and *matrices* (c3mat3, c3mat4) with various tools to manipulate them.
* c3quaternion: Quaternion implementation using c3algebra
* c3camera/c3arcball: camera manipulation, not perfect

The basic data structure is as follow:
* *c3context*:
	Mostly placeholder for now, hosts a "root" object, can reproject the
	objects & geometry, and call the callbacks to draw them.
* *c3object*: 
	* Has a list of (sub) c3objects
	* Has a list of c3transforms (ie matrices)
	* Has a list of c3geometry (ie real vertices and stuff)
  The object is a container for other objects, and for geometry itself. Objects don't
  necessary have geometry and/or sub objects, and don't even need transforms if their
  vertices are already projected.
* *c3geometry*:
	* Has a 'type' (raw for simple vertices, texture, triangles etc)
	* Has a 'subtype' (mostly can be used to draw GL types)
	* Has a 'material' (ie color, texture... to be completed)
	* Has a list of vertices
	* Has a list of texture coordinates (optional)
	* Has a list of vertices colors (optional)
	* Has a cached copy of a vertices when it has been 'projected'
* *c3transform*:
	Is just a sugar coated matrix, with an optional name.

Also there are:
* *c3pixels*:
	Is just a wrapper/holder for some pixels, either allocated, or inherited, 
	it's mostly used for *c3texture*
* *c3texture*:
	Associates a *c3geometry* with a *c3pixels* and has a standard Quad
	for vertices. The OpenGL drawing is not done there, it's done by the application using
	the generic *c3context* driver.
* *c3cairo*:
	Placeholder for now, inherits from *c3texture* and will contain a
	cairo surface mapped to a GL texture.
* *c3pango*:
	A text label, inherits from *c3cairo*

Draw Drivers "Inheritance"
------------
Various object uses static tables of callbacks to implement their behaviours
it's kinda cheap c++ inheritance, without the usual bloat.

There just a couple macros to call the driver chain for a particular function call.
The relevant bits are in c3driver*.h.

Mostly the code looks for a matching callback in a static table, and call it if found.
If that callback wants, it can also call the inherited object callback too.

Dirtyness
---------
There is a notion of 'dirtyness' in the tree, when you touch a *c3transform*, and/remove
objects and geometry, a dirty bit is propagated up the tree of object. This tells the
rendering it needs to reproject the dirty bits and repopulate the projected vertice
cache.

The 'dirty' bit moves both ways, when setting a dirty bit to true, it propagates upward,
when you set it to false, it propagates downward in the tree.
