import ctypes
from multiprocessing import shared_memory, resource_tracker

class ShmHeader(ctypes.Structure):
    _fields_ = [
        ("n", ctypes.c_int),  # length of shared_memory
        ("start_ts", ctypes.c_double),  # start timestamp
        ("interval", ctypes.c_double),  # interval between timestamps
        ("limit", ctypes.c_int),  # limit on number of elements
    ]

class ShmData(ctypes.Structure):
    _fields_ = [
        ("ts_", ctypes.c_double),
        ("v_", ctypes.c_int)
    ]

class ShmReader:
    def __init__(self, shm_name):
        try:
            self.shm = shared_memory.SharedMemory(name=shm_name, create=False)
            
            # Cast the buffer to a header structure using from_buffer_copy to avoid holding references
            self.header = ShmHeader.from_buffer_copy(self.shm.buf[:ctypes.sizeof(ShmHeader)])
            print(f"Header - n: {self.header.n}, start_ts: {self.header.start_ts}, "
                  f"interval: {self.header.interval}, limit: {self.header.limit}")
            
            # Unregister the shared memory from the resource tracker to avoid warnings
            resource_tracker.unregister(self.shm._name, "shared_memory")
        except FileNotFoundError:
            print("Shared memory does not exist.")
            raise
    
    def read_all(self):
        for i in range(self.header.n):
            data_dict = self.read(i)
            print(f"Index {i}: {data_dict}")
    
    def read(self, index):
        if not (0 <= index < self.header.n):
            raise IndexError("Index out of range")
        
        data_size = ctypes.sizeof(ShmData)
        offset = ctypes.sizeof(ShmHeader) + index * data_size
        
        # Use from_buffer_copy to avoid holding references
        data = ShmData.from_buffer_copy(self.shm.buf[offset:offset+data_size])
        return {'ts': data.ts_, 'v': data.v_}
    
    def detach(self):
        # Explicitly delete any references to objects created from the shared memory buffer
        if hasattr(self, 'header'):
            del self.header
        # Do not call self.shm.close() or self.shm.unlink()

if __name__ == '__main__':
    reader = None
    try:
        reader = ShmReader('/test_shm')
        reader.read_all()  # Print all data
    except Exception as e:
        print(e)
    finally:
        if reader is not None:
            reader.detach()