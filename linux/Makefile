VERSION = 1.0.0
PACKAGE_VERSION = 1
PACKAGE_NAME = chrome-stylus-pressure_$(VERSION)-$(PACKAGE_VERSION)

all: main.cpp
	mkdir -p build
	g++ main.cpp -o build/chrome-stylus-pressure -lX11 -lXi -DVERSION=$(VERSION)

package: all native.json package.txt
	mkdir -p build/$(PACKAGE_NAME)/DEBIAN
	mkdir -p build/$(PACKAGE_NAME)/usr/local/bin
	mkdir -p build/$(PACKAGE_NAME)/etc/opt/chrome/native-messaging-hosts
	mkdir -p build/$(PACKAGE_NAME)/etc/chromium/native-messaging-hosts
	cp ./package.txt build/$(PACKAGE_NAME)/DEBIAN/control
	cp ./build/chrome-stylus-pressure build/$(PACKAGE_NAME)/usr/local/bin/chrome-stylus-pressure
	cp ./native.json build/$(PACKAGE_NAME)/etc/opt/chrome/native-messaging-hosts/com.agamnentzar.stylus.json
	cp ./native.json build/$(PACKAGE_NAME)/etc/chromium/native-messaging-hosts/com.agamnentzar.stylus.json

	sed -i 's/VERSION/$(VERSION)-$(PACKAGE_VERSION)/g' build/$(PACKAGE_NAME)/DEBIAN/control
	fakeroot dpkg-deb --build build/$(PACKAGE_NAME)

clean:
	rm -rf build
