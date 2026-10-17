# C++ Style

- Use `static` functions instead of anonymous namespaces for file-local helpers.
- Make helper functions `constexpr` where possible.
- Byte-reading helpers should be templates sized by the type they read, e.g.
  `readBigEndian<T>` taking `std::span<uint8_t const, sizeof(T)>`, rather than
  fixed-width functions like `readBigEndian32`.
