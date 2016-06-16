FIES - Fault Injection for Evaluation of Software-based fault tolerance
==========================================================================

FIES is a QEMU fault injection extension.
 
The following picture shows the main points, where FIES takes action during an QEMU binary translation:
![alt tag](https://github.com/ahoeller/fies/blob/master/fies_doc/fies_tcg.png)


Building FIES
--------------

* Install required libraries: libffi, libiconv, gettext, python, pkg-config, glib, sdl, zlib, pixman, libfdt, libxml2

For detailed information about QEMU-required packages see http://wiki.qemu.org/Hosts/Linux
Additionally FIES requires `libxml2`

* Configure and build FIES
```splus
CF=$(xml2-config --cflags)
LF=$(xml2-config --libs)
./configure --target-list=arm-softmmu --extra-cflags=$CF --extra-ldflags=$LF --enable-sdl
cd pixman
./configure
cd ..
make
```

Using FIES
-----------

### Example files
For illustration we created a simple hello-world app and exemplary fault libraries in the folder `fies_sandbox 

### Compiling for FIES
Currently, FIES supports only ARM architectures. Thus, to compile an application that should be simulated with FIES compile it for ARM.

GCC example:
```splus
arm-none-eabi-gcc -marm *.c --specs=nosys.specs
```

Clang example:
```splus
clang -target arm -marm *.c 
```

If you use Code Sourcery use the following settings (C/C++ Build > Tool Settings)
* Board: QEMU ARM Simulator (VFP)
* Profile: Simulator
* Hosting: Hosted

### Starting an application without Fault Injection (for golden runs)

