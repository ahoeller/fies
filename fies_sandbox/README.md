FIES - Fault Injection for Evaluation of Software-based fault tolerance
==========================================================================

FIES is a QEMU fault injection extension.
 
The following picture shows the main points, where FIES takes action during an QEMU binary translation:
![alt tag](https://github.com/ahoeller/fies/blob/master/fies_doc/fies_tcg.svg)


Building FIES
--------------

* Install required libraries: libffi, libiconv, gettext, python, pkg-config, glib, sdl, zlib, pixman, libfdt, libxml2
  For detailed information about QEMU-required packages see http://wiki.qemu.org/Hosts/Linux . Additionally FIES requires `libxml2`.

* Configure and build FIES
```splus
CF=$(xml2-config --cflags)
LF=$(xml2-config --libs)
PP=$(which python2)
./configure --target-list=arm-softmmu --extra-cflags="$CF" --extra-ldflags="$LF" --python="$PP" --enable-sdl
cd pixman
./configure
cd ..
make
```

Using FIES
-----------

### Example files
For illustration we created a simple hello-world app and exemplary fault libraries in the folder `fies_sandbox`

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

### Execute an Application without Fault Injection (for Golden Runs)
Start the application the same way as you start a normal QEMU emulation (see http://wiki.qemu.org/download/qemu-doc.html#pcsys_005fquickstart)
Hint: to pass arguments use the `-append` flag

```splus
arm-softmmu/qemu-system-arm -semihosting -kernel <binary>
```

### Application Profiling
Use the `-profiling` flag to record register and memory usage

Flags:
* `m` profile memory usage
* `r` profile register usage

Results are stored in `profiling_meory.txt` or/and `profiling_registers.txt`

Example:
```splus
arm-softmmu/qemu-system-arm -semihosting -kernel <binary> -profiling rm
```

### Start Fault Injection
#### Define Fault Library
Faults that should be injected are described in an XML file.

XML fault lib example:
```splus
<?xml version="1.0" encoding="UTF-8"?>
<injection>
	<fault>
		<id>1</id>
		<component>RAM</component>
		<target>MEMORY CELL</target>
		<mode>SF</mode>
		<trigger>ACCESS</trigger>
		<type>PERMANENT</type>
		<params> 
			<address>0x07FFFFDC</address>
			<mask>0xFF</mask>
			<set_bit>0xFF</set_bit>
		</params>
	</fault>
</injection>
```

XML Fields:
* `<fault>`: Defines start and end of fault description. Multiple faults are injected concurrently if multiple fault descriptions are provided.
* `<id>`: Defines fault ID
* `<component>`: Defines the victim component (`CPU`, `RAM`, or `REGISTER`)
* `<target>`: Defines the target point of a fault as follows...
  * for `CPU` faults: `INSTRUCTION DECODER`, `INSTRUCTION EXECUTION`, or `CONDITION FLAGS`
