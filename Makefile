CC = clang
CFLAGS = -Wall -O2 -fobjc-arc
FRAMEWORKS = -framework Cocoa
DIST = dist
APP = $(DIST)/ColimaBar.app
SIGN_ID = colimabar
LABEL = com.manu.colimabar

all: $(APP) $(DIST)/$(LABEL).plist

$(APP): colimabar.m $(wildcard cb-*.h) Info.plist
	rm -rf $@
	mkdir -p $@/Contents/MacOS
	cp Info.plist $@/Contents/
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@/Contents/MacOS/colimabar colimabar.m
	codesign --force --sign $(SIGN_ID) $@

# One-time: self-signed identity so privacy grants survive rebuilds
cert:
	mkdir -p $(DIST)
	openssl req -x509 -newkey rsa:2048 -days 3650 -nodes -subj "/CN=$(SIGN_ID)" \
		-addext "keyUsage=critical,digitalSignature" -addext "extendedKeyUsage=codeSigning" \
		-keyout $(DIST)/cert.key -out $(DIST)/cert.crt
	openssl pkcs12 -export -legacy -inkey $(DIST)/cert.key -in $(DIST)/cert.crt \
		-out $(DIST)/cert.p12 -passout pass:x
	security import $(DIST)/cert.p12 -P x -T /usr/bin/codesign
	rm $(DIST)/cert.key $(DIST)/cert.crt $(DIST)/cert.p12

# LaunchAgent for installed/ColimaBar.app, copied into place with Finder (see README)
$(DIST)/$(LABEL).plist: $(LABEL).plist
	mkdir -p $(DIST)
	sed -e 's|@DIR@|$(CURDIR)|' -e 's|~/|$(HOME)/|' $< > $@

clean:
	rm -rf $(DIST)

.PHONY: all cert clean
