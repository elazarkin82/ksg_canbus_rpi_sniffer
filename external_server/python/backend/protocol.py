import ctypes

# --- Constants ---
CMD_CANBUS_DATA = 0x1001
CMD_CANBUS_TO_SYSTEM = 0x1002
CMD_CANBUS_TO_CAR = 0x1003
CMD_EXTERNAL_SERVICE_LOGGING_ON = 0x1004
CMD_EXTERNAL_SERVICE_LOGGING_OFF = 0x1005
CMD_SET_FILTERS = 0x1006
CMD_SET_PARAMS = 0x1007
CMD_KEEP_ALIVE_TO_SNIFFER = 0x1008
CMD_GET_PARAMS_REQ = 0x1009

CMD_CAN_MSG_FROM_SYSTEM = 0x2001
CMD_CAN_MSG_FROM_COMPUTER = 0x2002
CMD_CAN_MSG_BLOCKED_FROM_SYSTEM = 0x2003
CMD_CAN_MSG_BLOCKED_FROM_COMPUTER = 0x2004
CMD_KEEP_ALIVE_FROM_SNIFFER = 0x2005
CMD_GET_PARAMS_RES = 0x2006

FILTER_TARGET_BOTH = 0
FILTER_TARGET_TO_SYSTEM = 1
FILTER_TARGET_TO_CAR = 2

ACTION_PASS = 0
ACTION_DROP = 1
ACTION_MODIFY = 2

# --- Structures ---

class CanFilterRule(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("can_id", ctypes.c_uint32),
        ("data_index", ctypes.c_uint8),
        ("data_value", ctypes.c_uint8),
        ("data_mask", ctypes.c_uint8),
        ("action_type", ctypes.c_uint8),
        ("target", ctypes.c_uint8),
        ("modification_data", ctypes.c_uint8 * 8),
        ("modification_mask", ctypes.c_uint8 * 8)
    ]

class canfd_frame(ctypes.Structure):
    _fields_ = [
        ("can_id", ctypes.c_uint32),
        ("len", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
        ("res0", ctypes.c_uint8),
        ("res1", ctypes.c_uint8),
        ("data", ctypes.c_uint8 * 64)
    ]

class ExternalCanfdMessage(ctypes.Structure):
    _fields_ = [
        ("magic_key", ctypes.c_char * 4),
        ("frame", canfd_frame)
    ]

class KeepAliveToSnifferPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("logging_requested", ctypes.c_uint8),
        ("reserved", ctypes.c_uint8 * 31)
    ]

class KeepAliveFromSnifferPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("logging_active", ctypes.c_uint8),
        ("status_text", ctypes.c_char * 1024),
        ("reserved", ctypes.c_uint8 * 31)
    ]
