# CAScope
The most powerful and most performant tool to explore the world of one-dimensional cellular automata.

## Introduction
This is work in progress!
I may update the documentation, source code and build documentation from time to time.
If you have _serious_ interest in this software (wheather of scientific or recreational nature), you can contact me and I will be happy to help you out.
But please understand that this is a very niche project and I do not have the resources atm to present you a polished product.

## Building
CAScope only uses (Microsoft) C. You should be able to build it by using any recent version of MS Visual Studio. I currently use VS 2022, but VS 2017 and VS 2019 worked just fine in the past.
There are a few dependencies which are not included in this repository and that you need to provide yourself.
This should be no big deal if you have some experience with VS build options.
If you manage to build it yourself, please make some notes on the process and commit them to the project, so other users have an easier start.
You will find most of the relevant settings in VS here:
- Settings > Compiler > Additional Include Directories
- Settings > Linker > Additional Dependencies (lib files)
The dependencies are:
- SDL2 - https://www.libsdl.org/download-2.0.php
- SDL2_ttf - https://github.com/libsdl-org/SDL_ttf - I currently use v. 2.0.13
- OpenCL - https://www.khronos.org/opencl/ - \
  This is only used in some experimental code blocks, so if you do not want to install OpenCL, you should be able to just remove the OpenCL dependencies and then delete the few functions from `ca.c` that use them.\
  If you do want to include OpenCL use v. 1.2 and have a look here: https://stackoverflow.com/questions/28500496/opencl-function-found-deprecated-by-visual-studio



