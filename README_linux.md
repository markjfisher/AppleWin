# Building for linux

## Additional libraries required

The following libraries are required to be installed.

```
| Library Name | Arch Package | Ubuntu Packages        |
|--------------|--------------|------------------------|
| SLIRP        | libslirp     | libslirp-dev           |
|              |              | libslirp0              |
| Boost        | boost        | libboost-all-dev       |
| SDL2         | sdl2         | libsdl2-dev            |
|              |              | libsdl2-2.0-0          |
| SDL2_image   | sdl2_image   | libsdl2-image-dev      |
|              |              | libsdl2-image-2.0-0    |
```

For Ubuntu users, you can install all required packages with:
```bash
sudo apt update && sudo apt install libslirp-dev libslirp0 libboost-all-dev libsdl2-dev libsdl2-2.0-0 libsdl2-image-dev libsdl2-image-2.0-0
```

## building

```bash
git submodule update --init --recursive
# c = clean, b = build. use -h for help
./build.sh -cb
```
