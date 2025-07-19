class RefCounted:
    def __init__(self, obj, type_tag):
        self.obj = obj
        self.count = 1
        self.color = 'BLACK'
        self.type_tag = type_tag
        self.children = []  # Only for cycle detection

def incref(ref):
    ref.count += 1

def decref(ref):
    ref.count -= 1
    if ref.count == 0:
        collect_or_scan(ref)

def collect_or_scan(ref):
    if ref.color == 'BLACK':
        ref.color = 'GRAY'
        for child in ref.children:
            collect_or_scan(child)
        if all(child.count > 0 for child in ref.children):
            # Detected cycle
            reclaim_cycle(ref)
        else:
            free_ref(ref)

def reclaim_cycle(ref):
    # Reclaim all GRAY nodes in cycle
    pass
