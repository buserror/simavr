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

The data structure is as follow:
* *c3object*: 
	* Has a list of (sub) c3objects
	* Has a list of c3transforms (ie matrices)
	* Has a list of c3geometry (ie real vertices and stuff)
  The object is a container for other objects, and for geometry itself. Objects don't
  necessary have geometry and/or sub objects, and don't even need transforms if their
  vertices are already projected.
* *c3geometry*:
	* Has a 'type' (lines, quads, triangles..)
	* Has a 'material' (ie color, texture... to be completed)
	* Has a list of vertices
	* Has a list of texture coordinates (optional)
	* Has a list of vertices colors (optional)
	* Has a cached copy of a vertices when it has been 'projected'
* *c3transform*:
	Is just a sugar coated matrix, with an optional name.

Dirtyness
---------
There is a notion of 'dirtyness' in the tree, when you touch c3transform, and/remove
objects and geometry, a dirty bit is propagated up the tree of object. This tells the
rendering it needs to reproject the dirty bits and repopulate the projected vertice
cache.

The 'dirty' bit moves both ways, when setting a dirty bit to true, it propagates upward,
when you set it to false, it propagates downward in the tree.

"Inheritance"
-------------
There is a vague notion of inheritance for objects, where you can create more complex
ones and install a 'driver' (ie a function pointer table) that will be called to
perform various things. The skim is still evolving.

