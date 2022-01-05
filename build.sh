#!/bin/sh -e

echo "Cleaning.."
rm -f ./bin/uxnasm
rm -f ./bin/uxnemu
rm -f ./bin/uxncli
rm -f ./bin/boot.rom
rm -f ./bin/asma.rom

# When clang-format is present

if [ "${1}" = '--format' ]; 
then
	echo "Formatting.."
	clang-format -i src/uxn.h
	clang-format -i src/uxn.c
	clang-format -i src/devices/system.h
	clang-format -i src/devices/system.c
	clang-format -i src/devices/screen.h
	clang-format -i src/devices/screen.c
	clang-format -i src/devices/audio.h
	clang-format -i src/devices/audio.c
	clang-format -i src/devices/file.h
	clang-format -i src/devices/file.c
	clang-format -i src/devices/mouse.h
	clang-format -i src/devices/mouse.c
	clang-format -i src/devices/controller.h
	clang-format -i src/devices/controller.c
	clang-format -i src/uxnasm.c
	clang-format -i src/uxnemu.c
	clang-format -i src/uxncli.c
fi

mkdir -p bin
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c89 -Wall -Wno-unknown-pragmas}"
case "$(uname -s 2>/dev/null)" in
MSYS_NT*|MINGW*) # MSYS2 on Windows
	if [ "${1}" = '--console' ];
	then
		UXNEMU_LDFLAGS="-static $(sdl2-config --cflags --static-libs | sed -e 's/ -mwindows//g')"
	else
		UXNEMU_LDFLAGS="-static $(sdl2-config --cflags --static-libs)"
	fi
	;;
Darwin) # macOS
	CFLAGS="${CFLAGS} -Wno-typedef-redefinition"
	UXNEMU_LDFLAGS="$(brew --prefix)/lib/libSDL2.a $(sdl2-config --cflags --static-libs | sed -e 's/-lSDL2 //')"
	;;
Linux|*)
	UXNEMU_LDFLAGS="-L/usr/local/lib $(sdl2-config --cflags --libs)"
	;;
esac

if [ "${1}" = '--debug' ]; 
then
	echo "[debug]"
	CFLAGS="${CFLAGS} -DDEBUG -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined"
	CORE='src/uxn.c'
else
	CFLAGS="${CFLAGS} -DNDEBUG -Os -g0 -s"
	CORE='src/uxn.c'
fi

echo "Building.."
${CC} ${CFLAGS} src/uxnasm.c -o bin/uxnasm
${CC} ${CFLAGS} ${CORE} src/devices/system.c src/devices/file.c src/devices/mouse.c src/devices/controller.c src/devices/screen.c src/devices/audio.c src/uxnemu.c ${UXNEMU_LDFLAGS} -o bin/uxnemu
${CC} ${CFLAGS} ${CORE} src/devices/system.c src/devices/file.c src/uxncli.c -o bin/uxncli

if [ -d "$HOME/bin" ]
then
	echo "Installing in $HOME/bin"
	cp bin/uxnemu bin/uxnasm bin/uxncli $HOME/bin/
fi

echo "Assembling(boot).."
./bin/uxnasm projects/software/boot.tal bin/boot.rom
echo "Assembling(asma).."
./bin/uxnasm projects/software/asma.tal bin/asma.rom

if [ "${1}" = '--no-run' ]; then exit; fi

echo "Assembling(piano).."
bin/uxncli bin/asma.rom projects/examples/demos/piano.tal bin/piano.rom 2> bin/piano.log

echo "Running.."
cd bin
./uxnemu piano.rom

echo "Done."
cd ..
