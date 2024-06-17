### LibVermeer

Supposedly Vermeer used a camera obscura to paint his photo realistic portraits.  Therefore I thought that Vermeer was a fitting name for a rendering library.
Right now the project is turning into a just a platform to learn webgpu.

Developing simulation pipelines for personal use, one of the components that kind of intrigues me is immediate mode debugging.  Everyone roles their own way of
doing it.  I'd love to build something that worked in various contexts between forward rendering, deferred rendering all the way to full scale path tracing... 
OpenGL, Vulkan, WebGpu, you instantiate your visual debugger and it works across all contexts in all environments, and looks great doing it.  Not saying I'll 
get there with this project, but I'd like to make some inroads into that goal.

Right now I'm messing with a handful of things simultaneously.  I kind of hit a milestone then work on the next thing.
 - Wrapping Webgpu in a nice way based on some previous scene graphs I've worked on that might make using Rendering and Compute more seamless
 - Learning how to build AABB trees on the GPU for compute purposes... maybe extend whats out there from using standard ints to long ints
 - Path Tracing (using AABB trees)
 - Smooth Particle Hydrodynamics (using AABB trees, or Radix binning)
