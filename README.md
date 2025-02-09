
# zray : Can you Z the Rays?

I worked on the first iteration of this ray-tracer in 2002 with the naive idea that
I could make it run in real time. Well it never ran in real time but it was a fun
project and was happy with the results. 
There is no GPU here this is all ray CPU power just like the good old days :)

Roll on 2025 and I'm revisiting this and other old projects, getting them running
again, doing some refactoring, and generally correcting my 22 year old selves coding
blunders.

Click image to see video.
[![Watch the video](https://img.youtube.com/vi/65w-QslQVMg/maxresdefault.jpg)](https://youtu.be/65w-QslQVMg)

Because I was going for speed ZRay is not a full blown ray-tracer, and many sacrifices were made.
There is no recursion of rays (except lighting) that would provide features like reflection 
or refraction. Once a ray from the observer hits a surface, I simply check that point against
every light in the scene (10 in the demo) and calculate the colour using Lambert's cosine law.
Every pixel is rendered in the same way.

The demo scene consists of 34 planes and a single sphere (panda ball). 

My dev machine has an AMD Ryzen 9 3900X with 12 cores (it's about 5 years old now). With default
6 threads @720p I'm getting about 52fps but that's rendering 1/3 of the area. With subdivision = 1 
(press Z twice) and default 24 threads, I can get 14fps, with CPU utilization at 90% BLISTERING!
Still that's a large improvement on the several seconds it used to take on a single core.

I doubt anyone is doing this on the CPU when these types of calculations are perfect for modern GPU's
NIVIDIA OPTIX (CUDA only) or Vulkan Ray Tracing Extensions for 
example, neither of which I have really looked at much.


# Changes #

Now only using C++ standard libraries so it should be portable but I've only built it on windows.

It has now been ported from DirectDraw to [SDL3](https://github.com/libsdl-org/SDL).
SDL is providing us a screen buffer to draw pixels into, and keyboard interaction.

Added multithreading support. The rendering is now divided between threads. The number of threads
is selectable by the user at run time using the n & m keys.

Added spheres that we previously not implemented.

Fixed cross-product implementation that saw the x and z access reversed. Normal orientation is now... normal.

Creation of vector, colour, camera and other data structures and generally more consistent use
of data types.

Whole bunch of refactoring, comments.


# Issues #

I know about these issues.

Every now and then you may receive a page fault when existing the app. I have a feeling this is due to
how I'm trying to interrupt the running threads.

The shadowing does not currently do a check for ray-length bounds so you can get shadows on surfaces where they don't belong.

Another problem with the shadowing is the main light above the spinning cube does not cast a shadow?

The x64 executable download is being flagged by Chrome as having a virus??? It's not but you have been warned.


# What's next? #

Probably nothing! aside from a little tinkering.


# Download and Install #

Follow the link to download the working demo, includes X64 binary's and required
assets. Just extract files to a folder and run the executable.
[x64 downloaded here](https://204am.com/downloads/zray_2025.zip) md5:c09f54a421db24d15bb95afa2cf98cda
NOTE: The x64 executable download is being flagged by Chrome as being a virus??? It's not but you have been warned.


[textures only](https://204am.com/downloads/zray_textures.zip) md5:96b55dae3665763abd403103b252607b

This project requires the Microsoft Visual C++ Redistributable [downloaded here](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170) 

# Controls # 

keyboard controls and current settings are printed along the top of the window.


- Use arrow keys to move around.
- hold left shift (fast) or crtl (slow) movement.
- Y, H : To look up and down.
- O, P : change field of view.
- R : reset frame time minimum counter.
- N, M: adjust number of running threads
- Z, X: change screen subdivision (called step on screen)

The following toggle on/off
- A : anti-aliasing
- S : shadows
- L : lighting
- T : textures
- 1-0 : individual lights



# Compiling #

The project is built with Visual Studio 2022, and depends on [SDL3](https://github.com/libsdl-org/SDL).

The visual studio project assumes you have put this in C:\Libs\SDL\.

Follow the SDL docs\README.visualc.md for build instructions.

Add C:\Libs\SDL\VisualC\x64\Release to your environment variables or copy the 
SDL3.dll to the project folder either works.

Assuming it can find both of those my project "should" build and run just fine.


# Release # 

After doing a release build you just need to ship the executable with the release
build of C:\Libs\SDL\VisualC\x64\Release\SDL3.dll and the assets folder.


## The assets folder ##
Contains the textures for the demo. I found these online and have no idea where 
I got them now, if they are yours I'll take them down on request.


## The docs folder ##

The original source code for this project.


# Contact # 

I can be contacted at davez@204am.com