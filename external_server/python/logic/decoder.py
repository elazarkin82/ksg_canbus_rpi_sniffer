import struct

class Decoder:
    @staticmethod
    def decode(data, start_bit, length, type_name, scale=1.0, offset=0.0):
        # Simplified decoding logic (byte aligned for now)
        # TODO: Implement full bit-level extraction
        
        if len(data) == 0: return 0
        
        byte_idx = start_bit // 8
        if byte_idx >= len(data): return 0
        
        val = 0
        if type_name == "uint8":
            val = data[byte_idx]
        elif type_name == "uint16_be":
            if byte_idx + 1 < len(data):
                val = (data[byte_idx] << 8) | data[byte_idx+1]
        elif type_name == "uint16_le":
            if byte_idx + 1 < len(data):
                val = data[byte_idx] | (data[byte_idx+1] << 8)
        
        return (val * scale) + offset
