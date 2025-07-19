class TypePool:
    def __init__(self, type_tag, obj_size):
        self.type_tag = type_tag
        self.obj_size = obj_size
        self.free_list = []

    def allocate(self):
        if self.free_list:
            return self.free_list.pop()
        else:
            ptr = low_level_alloc(self.obj_size)
            pointer_registry[ptr] = PointerInfo(type_tag=self.type_tag)
            return ptr

    def free(self, ptr):
        self.free_list.append(ptr)

class PoolManager:
    def __init__(self):
        self.pools = {}

    def register_type(self, type_tag, obj_size):
        self.pools[type_tag] = TypePool(type_tag, obj_size)

    def alloc(self, type_tag):
        return self.pools[type_tag].allocate()

    def free(self, ptr, type_tag):
        self.pools[type_tag].free(ptr)
