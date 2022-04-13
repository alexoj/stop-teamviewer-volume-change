# STOP TEAMVIEWER VOLUME CHANGE

Quick and dirty program to patch out TeamViewer's less-than-ideal behavior
of reducing the volume of the microphone during meetings when it thinks you make
too much noise. (And then forcing that reduced volume every second, preventing you
from reverting it until you leave the meeting.)

This patches code in AudioSes.dll in memory to prevent TeamViewer's process from changing the
volume of the microphone. It does not touch TeamViewer's code itself so it
shouldn't (maybe?) trigger any anti-tamper measures.

Only tested in Windows 10 and probably only works thre.

Note that if you use this you do at your own risk. It is a shame that this is the
only way to get it to behave acceptably, but here we are.

This does **NOT** include the also less-than-ideal behavior of ducking your volume when
you start a remote control session, but that one is less annoying so I haven't gotten around
to finding out how to fix it.


## Building

Make sure to generate a Win32 executable even if you are in amd64.
```
mkdir _BUILD
cd _BUILD
cmake -G "Visual Studio 17 2022" -A Win32 ..
cmake --build .
```

## Usage

Just run the exe with TeamViewer running and it will work until you close it.