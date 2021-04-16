
hode README
Release version: 0.2.9f
-------------------------------------------------------------------------------


About:
------

hode is a reimplementation of the engine used by the game 'Heart of Darkness'
developed by Amazing Studio.


Datafiles:
----------

The original datafiles from the Windows releases (Demo or CD) are required.

- hod.paf (hod_demo.paf, hod_demo2.paf)
- setup.dat
- *_hod.lvl
- *_hod.sss
- *_hod.mst

See also the 'RELEASES.yaml' file for a list of game versions this program
has been tested with.


Running:
--------

By default the engine will try to load the files from the current directory
and start the game from the first level.

These defaults can be changed using command line switches :

    Usage: hode [OPTIONS]...
    --datapath=PATH   Path to data files (default '.')
    --savepath=PATH   Path to save files (default '.')
    --level=NUM       Start at level NUM
    --checkpoint=NUM  Start at checkpoint NUM

Display and engine settings can be configured in the 'hode.ini' file.

Game progress is saved in 'setup.cfg', similar to the original engine.


Credits:
--------

All the team at Amazing Studio for possibly the best cinematic platformer ever
developed.


Contact:
--------

Gregory Montoir, cyx@users.sourceforge.net


URLs:
-----

[1] https://www.mobygames.com/game/heart-of-darkness
[2] http://heartofdarkness.ca/
