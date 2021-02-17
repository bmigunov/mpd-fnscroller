mpd-fnscroller
Filename scroller

This program is intented to be used in conjunction with both MPD and i3blocks.
Main purpose is to display a file name of the song currently played in MPD on
the i3bar component and to manage the playback with i3blocks. If the file name
is too long to be displayed on the "block", it is supposed that it would be
"scrolled".

Dependencies
libmpdclient
mpc (for i3blocks scripts)
font-awesome (for icons)

Installation
It could be easily built from source:
make
sudo make install

Usage
First of all, there have to be appropriate scripts for the i3blocks. By
default, they are getting installed into the i3blocks scripts directory. mpd is
capable of displaying file name, LMB click stops the playback and updates
playlist, RMB click stops the playback. LMB click on mpd-nextbutton executes
"mpc next", LMB click on mpd-prevbutton executes "mpc prev", LMB click on
mpd-playpause executes "mpc toggle"
Next thing is to add the corresponding blocks into the i3blocks config, e.g.

[mpd-playpause]
min_width=
separator=false
interval=1

[mpd]
separator=false
interval=1
instance=16

[mpd-prevbutton]
interval=once
separator=false

[mpd-nextbutton]
interval=once
separator=true

Length of the displayed file name is configured by the "instance" property
passed to the mpd "block". Default value is 25 symbols.
