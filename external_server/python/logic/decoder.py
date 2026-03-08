import struct
import math

class Decoder:
    @staticmethod
    def decode(data, start_bit, length, type_name, scale=1.0, offset=0.0, formula=None):
        # Simplified decoding logic (byte aligned for now)
        
        if len(data) == 0: return 0
        
        # Handle Formula
        if type_name == "formula" and formula:
            try:
                # Safe environment for eval
                # 'd' is the data bytes array
                # 'm' is the math module
                env = {"d": list(data), "m": math, "int": int, "float": float}
                return eval(formula, {"__builtins__": {}}, env)
            except Exception as e:
                return f"Err: {e}"

        byte_idx = start_bit // 8
        if byte_idx >= len(data): return 0
        
        val = 0
        
        # --- Integers ---
        if type_name == "uint8":
            val = data[byte_idx]
        elif type_name == "int8":
            val = struct.unpack("b", bytes([data[byte_idx]]))[0]
            
        elif type_name == "uint16_be":
            if byte_idx + 1 < len(data):
                val = (data[byte_idx] << 8) | data[byte_idx+1]
        elif type_name == "uint16_le":
            if byte_idx + 1 < len(data):
                val = data[byte_idx] | (data[byte_idx+1] << 8)
        elif type_name == "int16_be":
            if byte_idx + 1 < len(data):
                val = struct.unpack(">h", data[byte_idx:byte_idx+2])[0]
        elif type_name == "int16_le":
            if byte_idx + 1 < len(data):
                val = struct.unpack("<h", data[byte_idx:byte_idx+2])[0]
                
        elif type_name == "uint32_be":
            if byte_idx + 3 < len(data):
                val = struct.unpack(">I", data[byte_idx:byte_idx+4])[0]
        elif type_name == "uint32_le":
            if byte_idx + 3 < len(data):
                val = struct.unpack("<I", data[byte_idx:byte_idx+4])[0]

        # --- Floats ---
        elif type_name == "float":
            if byte_idx + 3 < len(data):
                val = struct.unpack("f", data[byte_idx:byte_idx+4])[0]
                
        # --- Specials ---
        elif type_name == "percent":
            val = data[byte_idx]
            return f"{(val / 255.0) * 100:.1f}%"
            
        elif type_name == "ascii":
            try:
                # Decode from start_byte until end or null
                end_idx = byte_idx + (length // 8)
                if end_idx > len(data): end_idx = len(data)
                s = data[byte_idx:end_idx].decode('ascii', errors='ignore')
                return f"'{s}'"
            except:
                return "Err"

        # Apply Scale/Offset for numeric types
        if isinstance(val, (int, float)):
            res = (val * scale) + offset
            if isinstance(res, float):
                return f"{res:.2f}"
            return res
            
        return val
