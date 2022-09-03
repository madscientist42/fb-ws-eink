This is a WIP.  It fully works and well for full update passes at just under
2 seconds for a V3 2.13" display (What I have in hand...).  That being said,
there's a roadmap of things we intend on supporting, though there will not
be expected dates for these items as it's being worked on in a catch-as-catch-can
manner in mind as it's not a day-job project for me.  _Current_ plans are:

- Partial updates for the 2.13" display.
- Support for tri-color displays.
- Support for Adafruit's model of the same.
- Support for the 2.7" displays.
- Support for the 2.9" displays.

As the project progresses, and time permits, this list will move forward and
have other things added to it.  There's not really any good solutions for people
to actually cleanly _USE_ this stuff on a Linux system.  It's all this dodgy
kludgy Python or C code that is not cleanly unified and is specific down to a
version in the series of panels and if you get a new part from the vendor, you're
ripping out the code.  If you're using their stuff, you're limited in using their
library or rolling your own "driver" edge to things like LVGL and so forth.
It's this project's intent to, over time, make these interesting displays much
more usable and the roadmap reflects this intent.