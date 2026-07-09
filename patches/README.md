This directory contains patches to apply to the iPlug2 submodule before building Addivox.

`iplug2-popup-uaf`: fixes a crash that can happen when a popup menu (e.g. a dropdown) is open and the plugin window is closed at the same time. Closing the window destroys the plugin's UI, but the popup menu was still in the middle of doing something, so it ends up trying to use UI pieces that no longer exist, which crashes.
