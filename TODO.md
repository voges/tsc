# TODO

- Check code w.r.t. Google C++ Style Guide (pay particular attention to naming)`.
- Check code with ``cpplint.py``. (Write a Bash script for that which pulls ``cpplint.py`` via ``wget``.)
- zlib is an external dependency. Write a script (for openSUSE and Ubuntu) that installs this. (On Ubuntu the command is ``sudo apt-get install zlib1g-dev``.) Also check (in the ``CMakeLists.txt``?) that zlib is available for linking, and otherwise print an error message.
- In ``test/rans/rans_test.cc`` test with "peaky" geometric distributions.
- In all tests make sure to use a PRNG in combination with a fixed seed (to ensure reproducibility).
