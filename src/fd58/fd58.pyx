cimport fd58

cpdef enc32(bytes data):
    cdef char[45] out_buffer
    cdef unsigned length = 0
    if len(data) != 32:
        raise ValueError("Data length must be 32 bytes")
    cdef char* result = fd58.fd_base58_encode_32(<unsigned char*> data, &length, out_buffer)
    if result == NULL:
        raise MemoryError("Failed to encode Base58")
    return out_buffer[:length]

cpdef enc64(bytes data):
    cdef char[89] out_buffer
    cdef unsigned length = 0
    if len(data) != 64:
        raise ValueError("Data length must be 64 bytes")
    cdef char* result = fd58.fd_base58_encode_64(<unsigned char*> data, &length, out_buffer)
    if result == NULL:
        raise MemoryError("Failed to encode Base58")
    return out_buffer[:length]

cpdef dec32(bytes encoded):
    cdef unsigned char[32] out_buffer
    cdef unsigned char* result = fd58.fd_base58_decode_32(<const char*>encoded, out_buffer)
    if result == NULL:
        raise ValueError("Failed to decode Base58")
    return out_buffer[:32]

cpdef dec64(bytes encoded):
    cdef unsigned char[64] out_buffer
    cdef unsigned char* result = fd58.fd_base58_decode_64(<const char*>encoded, out_buffer)
    if result == NULL:
        raise ValueError("Failed to decode Base58")
    return out_buffer[:64]
