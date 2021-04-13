`hode` is a reimplementation of the engine used by the game 'Heart of Darkness'
developed by Amazing Studio.

Datafiles
=========
The original datafiles from the Windows releases (Demo or CD) are required.

- hod.paf (hod_demo.paf, hod_demo2.paf)
- setup.dat
- *_hod.lvl
- *_hod.sss
- *_hod.mst

See also the `RELEASES.yaml` file for a list of game versions this program
has been tested with.

Compiling
=========
Windows
-------
```
git clone --recurse-submodules https://github.com/CommonLoon102/hode-vs
cd hode-vs
3p\vcpkg\bootstrap-vcpkg.bat
cmake -B build -DCMAKE_BUILD_TYPE=Release -A x64 -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_TOOLCHAIN_FILE=%CD%\3p\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --parallel 3
```

Linux
-----
```
sudo apt update
sudo apt install libsdl2-dev git make cmake gcc g++ -y
git clone --recurse-submodules https://github.com/CommonLoon102/hode-vs
cd hode-vs
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
```

macOS
-----
```
brew upgrade
brew install sdl2
git clone --recurse-submodules https://github.com/CommonLoon102/hode-vs
cd hode-vs
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
```

Debugging
=========
Windows
-------
```
git clone --recurse-submodules https://github.com/CommonLoon102/hode-vs
cd hode-vs
3p\vcpkg\bootstrap-vcpkg.bat
```
1. **As admin** in the `hode-vs\3p\vcpkg` folder: `vcpkg.exe integrate install`
2. Open the `hode-vs` folder in Visual Studio 2019 (Get started -> Open a local folder)
3. Wait until CMake installs the packages via vcpkg
4. Next to the play button, select startup item: `hode.exe`
5. Click play
6. Copy the needed files described in the `Datafiles` section into the debug folder

Running
=======
By default the engine will try to load the files from the current directory
and start the game from the first level.

These defaults can be changed using command line switches :

    Usage: hode [OPTIONS]...
    --datapath=PATH   Path to data files (default '.')
    --savepath=PATH   Path to save files (default '.')
    --level=NUM       Start at level NUM
    --checkpoint=NUM  Start at checkpoint NUM

Display and engine settings can be configured in the `hode.ini` file.

Game progress is saved in `setup.cfg`, similar to the original engine.

Credits
=======
All the team at Amazing Studio for possibly the best cinematic platformer ever
developed.

Contact
=======
Gregory Montoir, cyx@users.sourceforge.net

URLs
====
- https://www.mobygames.com/game/heart-of-darkness
- http://heartofdarkness.ca/
