# Notes on external libraries

Static hives are file based on JSON format and using the https://github.com/DaveGamble/cJSON/ library. It is very small so can be used with embedded devices.

Dynamic hives use an embedded key/value store from the RocksDB library: https://rocksdb.org/docs/getting-started.html

