# ari-ppm-archivator
This is my decoder/encoder loseless archiving using methods such as prediction by partial matching and arithmetic compression with multiple coding tables and customizable aggressiveness.
# Building
For build this project in the repository directory use
```shell
  cmake src
  make
```
# Testing
Usage
```shell
  ./compress.exe [options]
```
Options
```shell
  --help                     = Print usage information and exit.
  --input  <file>            = Specify input file to compress/decompress.
  --output <file>            = Specify output file to write a result.
  --mode   {c | d}           = Use specified mode, `c` to compress and `d` to decompress.
  --method {ari | ppm}       = Use specified method of data compression/decompression.
```
Example
```shell
  ./compress.exe --input test.txt --output code.txt --method ari --mode c
```
# Configurating
You can change ```NUM_OF_TABLES``` and ```AGRESSION``` in ```src/config.h``` for arithmetic compressor. ```NUM_OF_TABLES``` is the number of character code tables, each of which uses its aggressiveness defined in the ```AGRESSION```, so that during encoding it is possible to choose the optimal option. Aggressiveness is responsible for the rate of change in character frequencies, which affects the rate of "forgetting" characters in a dynamically changing table. (It is necessary that the size of ```AGRESSION``` was equal to ```NUM_OF_TABLES```.)
