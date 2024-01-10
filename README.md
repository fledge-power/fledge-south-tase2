# TASE2 C/C++ South plugin

A simple asynchronous TASE2 plugin that pulls data from a server and sends it to Fledge.

To build this plugin, you will need the lib61850 library installed on your environment as described below.

You also need to have Fledge installed from the source code, not from the package repository.

## Building lib61850

To install the dependencies you can run the requirements.sh script.

If you want to install them manually :

To build TASE2 C/C++ Southth plugin, you need to download lib61850 at:
https://github.com/mz-automation/libtase2

```bash
$ git clone https://github.com/mz-automation/libtase2.git
$ cd libtase2
$ export LIB_TASE2=`pwd`
```
As shown above, you need a $LIB_TASE2 env var set to the source tree of the
library.

Then, you can build libtase2 with (note mbedtls installation):

```bash
$ cd libtase2
$ cd third-party/mbedtls
$ wget -c https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v2.28.3.tar.gz -O - | tar -xz
$ mv mbedtls-2.28.3 mbedtls-2.28
$ cd ../../
$ cmake -DBUILD_TESTS=NO -DBUILD_EXAMPLES=NO ..
$ make
$ sudo make install
$ sudo ldconfig
```

Build
-----


To build the tase2 plugin, once you are in the plugin source tree you need to run:

To build a release:

```bash 
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
```

To build with unit tests and code coverage:

```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Coverage ..
$ make
```

- By default the Fledge develop package header files and libraries
  are expected to be located in /usr/include/fledge and /usr/lib/fledge
- If **FLEDGE_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FLEDGE_ROOT directory.
  Please note that you must first run 'make' in the FLEDGE_ROOT directory.

You may also pass one or more of the following options to cmake to override
this default behaviour:

- **FLEDGE_SRC** sets the path of a Fledge source tree
- **FLEDGE_INCLUDE** sets the path to Fledge header files
- **FLEDGE_LIB sets** the path to Fledge libraries
- **FLEDGE_INSTALL** sets the installation path of Random plugin

NOTE:
- The **FLEDGE_INCLUDE** option should point to a location where all the Fledge
  header files have been installed in a single directory.
- The **FLEDGE_LIB** option should point to a location where all the Fledge
  libraries have been installed in a single directory.
- 'make install' target is defined only when **FLEDGE_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FLEDGE_ROOT set

  $ export FLEDGE_ROOT=/some_fledge_setup

  $ cmake ..

- set FLEDGE_SRC

  $ cmake -DFLEDGE_SRC=/home/source/develop/Fledge  ..

- set FLEDGE_INCLUDE

  $ cmake -DFLEDGE_INCLUDE=/dev-package/include ..
- set FLEDGE_LIB

  $ cmake -DFLEDGE_LIB=/home/dev/package/lib ..
- set FLEDGE_INSTALL

  $ cmake -DFLEDGE_INSTALL=/home/source/develop/Fledge ..

  $ cmake -DFLEDGE_INSTALL=/usr/local/fledge ..


Using the plugin
----------------

As described in the Fledge documentation, you can use the plugin by adding
a service from a terminal, or from the web API.C

1 - Add the service from a terminal:

.. code-block:: console

$ curl -sX POST http://localhost:8081/fledge/scheduled/task -d '{"name": "tase2","plugin": "tase2","type": "south","schedule_type": 3,"schedule_day": 0,"schedule_time": 0,"schedule_repeat": 30,"schedule_enabled": true}' ; echo

Or

2) Add the service from the web GUI:

- On the web GUI, go to the South tab
- Click on "Add +"
- Select tase2 and give it a name, then click on "Next"
- Change the default settings to your settings, then click on "Next"
- Let the "Enabled" option checked, then click on "Done"