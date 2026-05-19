# easyAPI Test Results

Generated from the local host build.

## Commands

```bash
cmake -S . -B build \
  -DEASY_API_BUILD_TESTS=ON \
  -DEASY_API_ENABLE_COVERAGE=ON \
  -DEASY_API_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/easyAPI_tests
gcov build/CMakeFiles/easyAPI.dir/src/easy_api.c.o \
     build/CMakeFiles/easyAPI.dir/src/easy_api_service.c.o
```

## Result

- Unit tests: 100% passed
- Examples: all configured examples compile
- Service-layer example runs successfully
- `src/easy_api.c`: 96.94% line coverage
- `src/easy_api_service.c`: 91.95% line coverage
- Combined easyAPI library coverage: 94.97% line coverage
