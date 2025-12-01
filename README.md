# Build and Run

```bash
cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/RedNoise
```

**Note:** Do not `cd` into the `build/` directory and run `./RedNoise` from there, as it will fail to read texture and OBJ files. Always run `./build/RedNoise` from the project root directory.
