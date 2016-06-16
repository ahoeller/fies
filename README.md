FAIL* - FAult Injection Leveraged
=======================================

FIES (Fault Injection for Evaluation of Software-based fault tolerance) is a QEMU fault injection extension.
 
The following picture shows the main points, where FIES takes action during an QEMU binary translation:
![alt tag](doc/)


Building FIES
--------------

* Install required libraries:
libffi, libiconv, gettext, python, pkg-config, glib, sdl, zlib, pixman, libfdt, libxml2

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

Using FIES*
-----------

### Example files
For illustration we created a simple hello-world app and exemplary fault libraries in the folder `fies_sandbox` 
