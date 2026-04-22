To compile:
```
 cmake -B build
 cmake --build build
```

if build fails run the first command as: 
```
 cmake -B build -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
```

and then run 
```
uriscv
```
to open the emulator
