n := win32-snake

mode := release
ifeq ($(mode), debug)
	cflags += -g
endif

ifeq ($(mode), release)
	cflags += -Oz
endif

libpath := $(shell misc/winsdk.exe --type:lib --arch:x64 --kit:um)

cflags += -std=c99
cflags += -Wall
cflags += -Wextra
cflags += -pedantic
cflags += -nostdlib
cflags += -ffreestanding
cflags += -fno-stack-check
cflags += -fno-stack-protector
cflags += -mno-stack-arg-probe
cflags += -fuse-ld=lld-link
cflags += -lkernel32
cflags += -lshell32
cflags += -luser32
cflags += -flto
cflags += -Xlinker /entry:start
cflags += -Xlinker /nodefaultlib
cflags += -Xlinker /subsystem:console
cflags += -Xlinker /libpath:"$(libpath)"

rcedit := "misc/rcedit/rcedit-x64.exe"

build: clean bin/$n.exe
	-

bin/$n.tiny.exe: src/$n.c bin Makefile
	clang $< $(cflags) -o $@
ifeq ($(mode), release)
	llvm-strip $@
endif

bin/$n.exe: bin/$n.tiny.exe
	./misc/cp.exe $< $@
	$(rcedit) $@ --set-icon misc/icon.ico

clean:
	-rd /s /q bin

bin:
	@-mkdir bin

.PHONY: build clean bin
