
 -- README --

AUTHORS

This software is (C) Abby Bratt and Kaj Bostrom, 2018-2019

INSTALLATION

In order to build the model, you will need to have a working C++ build system capable of supporting
at least the C++14 standard. You will also need the 'make' tool (available by default on most linux/Mac systems).

The following command will fetch the required dependencies and compile them (requires wget):

$ ./setup.sh

To compile the command-line executable (bin/headless):

$ make

To compile the graphical interface (bin/gui) (requires a recent version of the wxwidgets library to be installed):

$ make gui

USE

To run the model, run the command `bin/headless` from your terminal.
Alternately, if you have compiled the GUI, run `bin/gui`.
The model will load configuration from `default_config_env_from_file.json` unless a config file is specified as the first argument to the executable.

ADAPTING

In order to customize the behavior of the model for a given scenario, there are three main sets of parameters that you will likely
need to modify:
    Parameters that are built into the model and must be specified before compilation:
    - The bioenergetic parameters (defined as constants at the top of fish.cpp)
    - The hydrology model parameters (defined as constants at the top of hydro.cpp)

    Parameters that are specified via the JSON configuration file and are loaded at runtime:
    - The input data, including the map definition, recruitment data, and hydrology data
    For information on the format of these parameters see CONFIG_README.txt
