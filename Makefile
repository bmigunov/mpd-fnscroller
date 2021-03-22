SRC_DIR = src
EXECUTABLE = mpd-fnscroller


export EXECUTABLE




subsystem:
	cd $(SRC_DIR) && $(MAKE)

.PHONY: clean install

install: $(SRC_DIR)/$(EXECUTABLE)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(SRC_DIR)/$(EXECUTABLE) $(DESTDIR)/usr/local/bin/
	install -d $(DESTDIR)/lib/systemd/user
	install -m 644 sparse/lib/systemd/user/mpd-fnscroller.service $(DESTDIR)/lib/systemd/user
	install -d $(DESTDIR)/etc/default
	install -m 644 sparse/etc/default/mpd-fnscroller $(DESTDIR)/etc/default
	install -d $(DESTDIR)/usr/share/i3blocks
	install -m 755 sparse/usr/share/i3blocks/mpd $(DESTDIR)/usr/share/i3blocks
	install -m 755 sparse/usr/share/i3blocks/mpd-nextbutton $(DESTDIR)/usr/share/i3blocks
	install -m 755 sparse/usr/share/i3blocks/mpd-playpause $(DESTDIR)/usr/share/i3blocks
	install -m 755 sparse/usr/share/i3blocks/mpd-prevbutton $(DESTDIR)/usr/share/i3blocks
	install -m 755 sparse/usr/share/i3blocks/mpd-repeat $(DESTDIR)/usr/share/i3blocks
	install -m 755 sparse/usr/share/i3blocks/mpd-shuffle $(DESTDIR)/usr/share/i3blocks

clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(SRC_DIR)/$(EXECUTABLE)
