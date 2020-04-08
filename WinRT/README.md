# Heart of Darkness - WinRT / UWP

## Requirements

* Visual Studio 2019
* VS needs UWP / WinRT support support enabled

## Build

### Clone the repo and its dependencies

```
git clone https://github.com/tuxuser/hode.git
git checkout winrt
git submodule update --init --recursive
```

### Visual studio build
* Copy Heart of Darkness gamedata to data/ directory (setup.dat without a subfolder)
* Open <REPO>/WinRT/HeartOfDarkness.sln in Visual Studio
* Right-click on "SDL2-UWP" in "Solution Explorer", hit "Retarget projects", confirm with OK
* Build the project
* Deploy locally or to a remote device (Xbox One maybe?)
  
  NOTE: When deploying to Xbox One you need to be signed in with an account on the console.

## Required data files

Prior building, these files need to copied inside the project:

```
data/Dark_hod.lvl
data/Dark_hod.mst
data/Dark_hod.sss
data/Fort_hod.lvl
data/Fort_hod.mst
data/Fort_hod.sss
data/Isld_hod.lvl
data/Isld_hod.mst
data/Isld_hod.sss
data/Lar1_hod.lvl
data/Lar1_hod.mst
data/Lar1_hod.sss
data/Lar2_hod.lvl
data/Lar2_hod.mst
data/Lar2_hod.sss
data/Lava_hod.lvl
data/Lava_hod.mst
data/Lava_hod.sss
data/Pwr1_hod.lvl
data/Pwr1_hod.mst
data/Pwr1_hod.sss
data/Pwr2_hod.lvl
data/Pwr2_hod.mst
data/Pwr2_hod.sss
data/Rock_hod.lvl
data/Rock_hod.mst
data/Rock_hod.sss
data/Setup.dat
data/hod.paf
```
