class Region:
    def __init__(self, id):
        self.id = id
        self.allocations = []

class RegionManager:
    def __init__(self):
        self.regions = {}
        self.next_id = 0

    def region_begin(self):
        rid = self.next_id
        self.regions[rid] = Region(rid)
        self.next_id += 1
        return rid

    def region_alloc(self, rid, size, type_tag):
        ptr = low_level_alloc(size)
        self.regions[rid].allocations.append((ptr, type_tag))
        pointer_registry[ptr] = PointerInfo(type_tag=type_tag, region_id=rid)
        return ptr

    def region_free(self, rid):
        for (ptr, _) in self.regions[rid].allocations:
            low_level_free(ptr)
            pointer_registry.pop(ptr, None)
        del self.regions[rid]
