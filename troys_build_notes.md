### Mac build notes

repo: [https://github.com/noaa-nwfsc/Skagit-IBM](https://github.com/noaa-nwfsc/Skagit-IBM)
- follow README instructions (on home page)
- you may need to reference [[Homebrew installation]]
- several of the kegs poured by Hombrew were "keg only", meaning that they were not configured to override the system defaults. Here are the relevant messages:
```
==> curl
curl is keg-only, which means it was not symlinked into /Users/troy.frever/homebrew,
because macOS already provides this software and installing another version in
parallel can cause all kinds of trouble.

If you need to have curl first in your PATH, run:
  echo 'export PATH="/Users/troy.frever/homebrew/opt/curl/bin:$PATH"' >> ~/.zshrc

For compilers to find curl you may need to set:
  export LDFLAGS="-L/Users/troy.frever/homebrew/opt/curl/lib"
  export CPPFLAGS="-I/Users/troy.frever/homebrew/opt/curl/include"

zsh completions have been installed to:
  /Users/troy.frever/homebrew/opt/curl/share/zsh/site-functions
==> libtool
All commands have been installed with the prefix "g".
If you need to use these commands with their normal names, you
can add a "gnubin" directory to your PATH from your bashrc like:
  PATH="/Users/troy.frever/homebrew/opt/libtool/libexec/gnubin:$PATH"
==> zlib
zlib is keg-only, which means it was not symlinked into /Users/troy.frever/homebrew,
because macOS already provides this software and installing another version in
parallel can cause all kinds of trouble.

For compilers to find zlib you may need to set:
  export LDFLAGS="-L/Users/troy.frever/homebrew/opt/zlib/lib"
  export CPPFLAGS="-I/Users/troy.frever/homebrew/opt/zlib/include"
```

I have not made any of the above modifications, and Homebrew is working correctly for me.

The `setup.sh` file wasn't quite right for a mac. Firstly, [RapidJSON](https://rapidjson.org/) needs to be installed. From the root of the local Skagit-IBM repository:
```
cd ..
git clone https://github.com/Tencent/rapidjson.git
cd Skagit-IBM
ln -s ../rapidjson .
```
I was unable to compile the Rapidjson tests, probably because the current Clang version is much newer than what they list as supporting. Fortunately Rapidjson still works in our project.

If necessary, add execute permissions on the setup script: 
`chmod +x ./setup.sh` 

I updated the netcdf portion of the setup script, which now does a full download and install. However, one should typically run `make check` manually in `Skagit-IBM/netcdf-c/netcdf-c-4.9.2`. At the moment this results in 3 test failures. Hopefully not a big problem. Note also that the setup script for netcdf includes a Mac bugfix (check the comments).

```
/Applications/Xcode.app/Contents/Developer/usr/bin/make  check-TESTS
PASS: run_ut_chunk.sh
PASS: run_ut_map.sh
PASS: run_ut_mapapi.sh
PASS: run_ut_misc.sh
PASS: run_ncgen4.sh
PASS: run_fillonlyz.sh
PASS: run_chunkcases.sh
PASS: run_quantize.sh
PASS: run_purezarr.sh
PASS: run_interop.sh
PASS: run_misc.sh
PASS: run_nczarr_fill.sh
PASS: run_jsonconvention.sh
PASS: run_strings.sh
PASS: run_scalar.sh
PASS: run_nulls.sh
PASS: run_nczfilter.sh
FAIL: run_filter.sh
FAIL: run_unknown.sh
FAIL: run_specific_filters.sh
============================================================================
Testsuite summary for netCDF 4.9.2
============================================================================
# TOTAL: 20
# PASS:  17
# SKIP:  0
# XFAIL: 0
# FAIL:  3
# XPASS: 0
# ERROR: 0
============================================================================
See nczarr_test/test-suite.log for debugging.
Some test(s) failed.  Please report this to support-netcdf@unidata.ucar.edu,
together with the test-suite.log file (gzipped) and your system
information.  Thanks.
============================================================================
make[4]: *** [test-suite.log] Error 1
make[3]: *** [check-TESTS] Error 2
make[2]: *** [check-am] Error 2
make[1]: *** [check] Error 2
make: *** [check-recursive] Error 1
```

Thus far I have not found it necessary to correct the above errors.

### run
The run section of the Readme is not up to date. Here is what is working for me:
Make a run9 directory to hold the output (you only need to do this once).
```
mkdir run9
```

Run the model from the base directory.
```
./bin/Debug/headless "test_run_listings_reepicheep.csv" "run9/map2013d2014" "run6_config/config_map_2013_data_2014.json"
```

When invoking the GUI from the command line, the parameter order is different (the config json comes first):
```
bin/Debug/gui "run6_config/config_map_2013_data_2014.json" "test_run_listings_reepicheep.csv" "run9/map2013d2014"
```

If using CLion, simply point the IDE to the CMakeLists.txt file in the project root when opening the project for the first time.

### Python, Jupyter, Conda, and using .nc files (NetCDF)
- Install [miniconda](https://docs.anaconda.com/miniconda/).  This is simply the command-line only version of conda. You could install the full GUI version of Anaconda if you prefer.
- Get a conda environment file, such as `skagit_env.yml` from someone. Before creating the conda environment, I had to remove all build numbers and a few of the version numbers from the yaml file. I also removed `mkl`, `mkl-service`, and `intel-openmp`, since these don't have arm64 versions. From the `pip` section, remove `install`, and remove version numbers from `basemap`, `basemap-data`, `matplotlib`, and `matplotlib-inline`.
- In the future, exporting the yaml file using `conda env export --from-history`, which will only include packages explicitly requested and re-determine all the dependencies for the new platform.
- To create and activate the environment, use 
```
conda env create -f skagit_env.yml
conda activate netcdf
```
The instructions for [creating a conda env from yaml](https://saturncloud.io/blog/how-to-create-a-conda-environment-based-on-a-yaml-file-a-guide-for-data-scientists/) contain more information. The [conda documentation](https://docs.conda.io/projects/conda/en/latest/user-guide/tasks/manage-environments.html#creating-an-environment-with-commands) on environments is also helpful. 
- Make sure the Jupyter Notebook kernel is configured properly, following [these instructions](https://www.python-engineer.com/posts/setup-jupyter-notebook-in-conda-environment/).
- To get the conda environment working in PyCharm, I followed [these instructions](https://mariashaukat.medium.com/a-guide-to-conda-environments-in-jupyter-pycharm-and-github-5ba3833d859a) to point the python interpreter to my new conda environment.
