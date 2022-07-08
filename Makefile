.PHONY: default build install run clean

default: run

build:
	meson build
	meson compile -C build

install: build
	meson install -C build

run: build
	./build/i-spy-notify

clean:
	rm -rf build
