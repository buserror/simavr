**libc3** - No frill 'scene' graph library in C
=====
(C) 2012 Michel Pollet <buserror@gmail.com>

**WARNING** This API is not your nanny. It is made to be lean, mean, efficient
with no frill, no asserts, no bounds checking, no sugar coating.

On the other hand it's fast, reasonably clean and is a micro-fraction of the
other giganormous 'scene graphs' or 'game engine' libraries around.

It's vaguely inspired by THREE.js funnily enough, because it allows you to
hack around and quickly get stuff on screen with the minimal amount of 
effort.

Introduction
-----------
The general idea is that the library keeps track of geometry and stuff, but doesn't
do *any* opengl or related calls. Instead, it uses callbacks into code that will
take care of the rendering related tasks.

So for example a c3pixels represents a texture, but a callback into the rendering
layer will be responsible to push the pixels to OpenGL, and store the object back
into the c3pixels for reference.

Status
-------
The API is generally functional, but it's brand new. I try not to add bits that
I aren't needed, and I also don't add stuff that isn't tested.

There is an ASCII STL file loader that works, and a few other bit of geometry related
helpers. 

It's currently used in one 'serious' project and also in my [3D printer simulator](https://github.com/buserror-uk/simavr/tree/master/examples/board_reprap),
as part of simavr. There you cal also find the "opengl renderer" set of callbacks, in the
near future, this layer will be part of a *libc3-gl* companion library. 

General Roadmap
---------------
There is a [PDF Flowchart](https://github.com/buserror-uk/libc3/raw/master/doc/libc3-flowchart.pdf) 
of how things are mostly organized as far as data structure goes, but the following is a
breakdown of the major components.

The API has various bits:
* c3algebra: C derivative of an old C++ piece of code I had lying around and that has
been present in my toolset for a long time. It gives you *vectors* (c3vec2, c3vec3, c3vec4)
and *matrices* (c3mat3, c3mat4) with various tools to manipulate them.
* c3quaternion: Quaternion implementation using c3algebra
* c3camera/c3arcball: camera manipulation bits

The basic data structure is as follow:
* *c3context*:
	Hosts a "root" object, and a list of 'viewpoints' (ie either cameras, or lights).
	it can reproject the objects & geometries, and call the callbacks to draw them.
	
	The context also keeps a list of *c3pixels* and *c3program* that are referenced
	by the geometries.
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
	* Has a 'material' (ie color, texture, a GPU program... to be completed)
	* Has a list of vertices
	* Has a cached copy of a vertices when it has been 'projected'
	* Has a list of texture coordinates (optional)
	* Has a list of vertices colors (optional)
	* Had a list of vertices indexes (optional)
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
