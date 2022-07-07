# ari-ppm-archivator
This is my decoder/encoder loseless archiving using methods such as prediction by partial matching and arithmetic compression with multiple coding tables and customizable aggressiveness.
# Building
For build this project in the repository directory use
```bibtex
  cmake src
  make
```
# Testing
Usage
```bibtex
  ./compress.exe [options]
```
Options
```bibtex
  --help                     = Print usage information and exit.
  --input  <file>            = Specify input file to compress/decompress.
  --output <file>            = Specify output file to write a result.
  --mode   {c | d}           = Use specified mode, `c` to compress and `d` to decompress.
  --method {ari | ppm}       = Use specified method of data compression/decompression.
```
Example
```bibtex
  ./compress.exe --input test.txt --output code.txt --method ari --mode c
```
