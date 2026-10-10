# What the release archives require

Every archive attached to a release is built on the oldest system we are willing
to support for that platform, and what it ends up demanding is measured after
the build rather than assumed. The numbers below come from `check-floor.sh`,
which the release workflow runs over every ELF object in the archive, not just
over the executable: a bundled library can demand more than the program does,
and the floor of an archive is the maximum over everything in it.

| Archive | Requires | Found on |
| ------- | -------- | -------- |
| `simavr-linux_x86_64.tar.gz` | GLIBC 2.34 | Ubuntu 22.04, Debian 12, RHEL 9 and later |
| `simavr-linux_i686.tar.gz` | GLIBC 2.34 | Ubuntu 22.04, Debian 12 and later |
| `simavr-linux_aarch64.tar.gz` | GLIBC 2.17 | RHEL 7, Raspberry Pi OS bullseye and later |
| `simavr-linux_armv6l.tar.gz` | GLIBC 2.28, ARMv6, hard float | Raspberry Pi OS buster and later, Pi 1 and Zero included |
| `simavr-linux_armv7l.tar.gz` | GLIBC 2.28, ARMv7, hard float | Debian 10, Raspberry Pi OS buster and later |
| `simavr-darwin_x86_64.tar.gz` | macOS 10.15 | Intel Macs |
| `simavr-darwin_arm64.tar.gz` | macOS 11.0 | Apple Silicon |
| `simavr-windows_amd64.tar.gz` | Windows 10 | 64 bit only |

GLIBC is backward compatible, so a newer system runs these binaries as well. The
arm archives ask for less than the system they were built on (Raspberry Pi OS
bullseye, GLIBC 2.31) because the code only references older symbols.

## Nothing else has to be installed

Each archive carries what it needs. On Linux that means `libsimavr`, and beside
it `libelf` and `libz`, found through an RPATH of `$ORIGIN`; only the C library
itself comes from the system. On macOS `libelf` and `libsimavr` are linked
statically and the binary names nothing but `/usr/lib/libSystem.B.dylib`. On
Windows the Universal CRT comes from the operating system, which is what makes
Windows 10 the floor, and any other DLL sits next to `simavr.exe`.

The graphical parts of simavr are not in these archives. `make install-simavr`
installs the simulator and its library, not `libsimavrparts`, so no archive
depends on OpenGL, GLUT or X11 and the simulator runs on a machine without a
display.

## What is not included

There is no DWARF support: `libdwarf` changes its API between releases, and the
version on the oldest system we build on is older than the sources expect. The
`info io_registers` command of simavr therefore does not work in these builds.
Everything a debugger needs is read by the debugger itself from the firmware's
ELF file, so a normal GDB session is unaffected.

The binaries are not stripped. They carry their debug information, which makes
the archives a few hundred kilobytes larger and a crash report useful.

## Checking it yourself

```
./check-floor.sh bin/simavr 2.34          # ELF:    highest GLIBC version referenced
./check-floor.sh bin/simavr 11.0          # Mach-O: deployment target
./check-floor.sh bin/simavr.exe 10.0 --ucrt-ok   # PE: the DLLs it imports
```

The script fails when a binary demands more than the version given, which is how
the release workflow notices the day a rebuilt archive would quietly raise its
own floor.
