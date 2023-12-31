# window_tiler

https://github.com/oconnorkg/window_tiler/releases/download/1.0/window_tiler.exe

This solves a couple of problems I was having:
 - Sometimes I lose windows when I change monitor configuration. They're off somewhere out in the ether. Sometimes even the WinKey-Left thing doesn't get them back. I just want them brought back to where my cursor is, every time without fuss.
 - My window positions and sizes are sometimes messed up by any monitor changes I might make, or just by Windows when I log in. I want them remembered so I don't have to keep rearranging them.

I couldn't find anything around that solved these problems reliably with multiple monitors. So I wrote this.
It's just for Windows, and it's meant to run in the background all the time. The exe's fairly small (35k), quite quick, and most of the time does nothing except wait for a hotkey event. On my machine I put a shortcut to it in my StartUp folder so it runs every time I reboot (WinKey-R and type "shell:startup" to find the StartUp folder).

## Usage:
 - CTRL-ALT-G : Gathers all the windows on your desktop to the top corner of the monitor your mouse is currently over.
 - CTRL-ALT-SHIFT-1 -> CTRL-ALT-SHIFT-9 : Saves all your desktop window positions and sizes into one of these 9 slots.
 - CTRL-ALT-1 -> CTRL-ALT-9 : Restores all your desktop window positions and sizes, that you must have previously saved into one of these 9 slots.
 
## Building:
 - There's a pre-built exe over on the right of this page under "Releases". At the moment Windows will block you running it. I'm looking into how to fix this, but until then you can unblock it by right-clicking the exe, selecting Properties and clicking the Unblock checkbox, then OK.
 - But if you want to build from source...
 - I've included the Visual Studio 2022 solution, but you can just grab the .cpp file and build it in whatever - the only thing to remember in compiler settings is it's using the Multi-Byte Character Set, not Unicode (Visual Studio 2022: Project->Settings->Advanced->Character Set).
