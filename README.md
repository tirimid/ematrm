# ematrm

## Introduction

EMatRM, or (E)soteric (Mat)rix (R)egister (M)achine, is an interpreter for an
esoteric programming language by its same name. It is not designed for serious
usage and was thrown together quickly and hackily because my brain just wouldn't
let me drop the idea. It is terribly stupid - but, then again, it is an esolang,
so that is to be expected.

## Dependencies

System / software dependencies are:

* A C++20 compiler (for build)
* Make (for build)
* A shell environment (for program execution)

## Management

* To build EMatRM, run `make`
* To delete build files, run `make clean`
* To install EMatRM after building, run `make install`
* To uninstall EMatRM after installation, run `make uninstall`

## Usage

After installation, you can interpret an EMatRM source file as follows:

```
$ ematrm <file.emat>
```

## Contributing

Do not bother contributing. Feel free to study the source code and make your own
fork of this project. I will most likely never seriously work on this again
after today (as of writing this).
