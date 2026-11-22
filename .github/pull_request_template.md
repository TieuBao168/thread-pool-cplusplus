## Description
<!-- A short description of the change and why it is needed. -->

## Related Issue
<!-- e.g. Fixes #123 -->

## How to Test
```bash
cmake -S . -B build -DTP_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Checklist
- [ ] Builds without warnings (`TP_WARNINGS_AS_ERRORS=ON`)
- [ ] Tests added or updated, and `ctest` passes
- [ ] Public API changes are reflected in `README.md`
- [ ] New config keys are documented in `config/threadpool.ini.example`
