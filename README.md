### LibVermeer

Supposedly Vermeer used a camera obscura to paint his photo realistic portraits.  Therefore I thought that Vermeer was a fitting name for a rendering library.
Right now the project is turning into a just a platform to learn webgpu.

Right now I'm messing with a handful of things simultaneously.  I kind of hit a milestone then work on the next thing.
 - Wrapping Webgpu in a nice way based on some previous scene graphs I've worked on that might make using Rendering and Compute more seamless
 - Learning how to build AABB trees on the GPU for compute purposes... maybe extend whats out there from using standard ints to long ints
 - Path Tracing (using AABB trees)
 - Smooth Particle Hydrodynamics (using AABB trees, or Radix binning)
