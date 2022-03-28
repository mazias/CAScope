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

## Usage
CAScope can only be controlled by the keyboard.
You will get an **overview of the keyboard controls by pressing F1**.
But not all controls are listed there.
I am planning to update the documentation, but if you are curious have a look on the keyboard control code in the function CA_MAIN in file ca.c and look for `(e.type == SDL_KEYDOWN)`

All the settings are saved in `settings.txt` when you **close the program by pressing F4**. The settings are loaded when CAScope is started. You can easily edit this file with any text editor!

## Screenshots
![](screenshots/TS-3N-3%23-e4R-3019144G-1920SX.png)
![](screenshots/TS-3N-3%23-e4R-6234752G-1700SX.png)
![](screenshots/TS-3N-3%23-e4R-6260750G-1920SX.png)
![](screenshots/TS-3N-3%23-e4R-10168920G-1920SX.png)
![](screenshots/TS-3N-3%23-e4R-10170662G-1700SX.png)
![](screenshots/TS-3N-3%23-e4R-10894464G-1366SX.png)
![](screenshots/TS-3N-3%23-e4R-13789814G-1366SX.png)
![](screenshots/TS-3N-3%23-e4R-20999125G-1920SX.png)
![](screenshots/TS-3N-3%23-e4R-857353169G-1920SX.png)
![](screenshots/TS-3N-3%23-e7R-1027093G-500SX.png)
![](screenshots/TS-3N-4%23-3f0e5R-308631G-1366SX.png)
![](screenshots/TS-3N-4%23-6e93aR-35283G-1366SX.png)
![](screenshots/TS-3N-4%23-15dceR-2212619G-1267SX.png)
![](screenshots/TS-3N-4%23-471f6R-229852G-1366SX.png)
![](screenshots/TS-3N-4%23-471f6R-1015600G-1366SX.png)
![](screenshots/TS-3N-4%23-bb007R-2184189G-1366SX.png)
![](screenshots/TS3N4%23f45c4-1124.png)
![](screenshots/TS-3N-3%23-18aR-181390G-1920SX.png)
![](screenshots/TS-3N-3%23-56fR-2806362G-1700SX.png)
![](screenshots/TS3N3%2365-63067.png)
![](screenshots/TS3N3%2379f-107574.png)
![](screenshots/TS-3N-3%23-102R-404500G-1267SX.png)
![](screenshots/TS-3N-3%23-412R-21817637G-1821SX.png)
![](screenshots/TS-3N-3%23-60418bd26a0R-10947615G-1821SX.png)
![](screenshots/TS-3N-3%23-e4R-90435G-1920SX.png)
![](screenshots/TS-3N-3%23-e4R-640618G-1700SX.png)
