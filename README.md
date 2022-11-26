# imgnow
imgnow is a lightweight image viewer that aims to
open image files as fast as possible with no fuss.

# Features
- Pan, zoom, rotate, and flip.
- Show pixel grid.
- Inspect pixel data.
- Copy section to clipboard.
- Copy colour in multiple formats.

# Building
```
git clone https://github.com/kevlu123/imgnow.git
cd imgnow
git submodules update --init --recursive
```
Then follow one of the following methods:

- Open the project folder in Visual Studio and build.
- In a terminal

        cmake -S. -Bout
        cmake --build out
