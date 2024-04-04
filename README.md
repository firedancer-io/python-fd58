python-fd58 bundles the fast Base58 from Firedancer as a Python package.

Source: https://github.com/firedancer-io/firedancer/tree/main/src/ballet/base58

Supports 32 and 64 byte data only.

Usage:

```python
>>> import fd58
>>> fd58.enc32(b'\x00' * 32)
b'11111111111111111111111111111111
>>> fd58.dec32(b'11111111111111111111111111111111')
b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
```
