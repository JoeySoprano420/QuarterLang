# memory_inspector.py

import gc
import inspect
import pickle
import time
import threading
import tracemalloc
from collections import defaultdict
from types import FrameType

pointer_registry = {}  # Maps id(obj) -> metadata
pointer_history = []   # List of (timestamp, alloc/free, type_tag, size, caller_fn)
arena_segments = defaultdict(list)  # thread_id -> [obj ids]

# Enable tracing memory allocations
tracemalloc.start()

class MemoryInspector:
    @staticmethod
    def capture_snapshot(filename='snapshot.pkl'):
        snapshot = tracemalloc.take_snapshot()
        with open(filename, 'wb') as f:
            pickle.dump(snapshot, f)
        return snapshot

    @staticmethod
    def load_snapshot(filename='snapshot.pkl'):
        with open(filename, 'rb') as f:
            snapshot = pickle.load(f)
        return snapshot

    @staticmethod
    def list_objects():
        return [o for o in gc.get_objects() if hasattr(o, '__class__')]

    @staticmethod
    def save_repl_session(filename='repl_session.pkl'):
        session = {k: v for k, v in globals().items() if not k.startswith('__')}
        with open(filename, 'wb') as f:
            pickle.dump(session, f)

    @staticmethod
    def record_allocation(obj):
        obj_id = id(obj)
        type_tag = type(obj).__name__
        caller = inspect.stack()[2].function
        pointer_registry[obj_id] = {
            'type': type_tag,
            'time': time.time(),
            'caller': caller,
            'size': getattr(obj, '__sizeof__', lambda: 0)(),
            'thread_id': threading.get_ident()
        }
        pointer_history.append((time.time(), 'alloc', type_tag, pointer_registry[obj_id]['size'], caller))
        arena_segments[threading.get_ident()].append(obj_id)

    @staticmethod
    def record_deallocation(obj):
        obj_id = id(obj)
        if obj_id in pointer_registry:
            meta = pointer_registry.pop(obj_id)
            pointer_history.append((time.time(), 'free', meta['type'], meta['size'], meta['caller']))
            if obj_id in arena_segments[meta['thread_id']]:
                arena_segments[meta['thread_id']].remove(obj_id)

    @staticmethod
    def arena_free(thread_id):
        for obj_id in arena_segments[thread_id]:
            if obj_id in pointer_registry:
                meta = pointer_registry[obj_id]
                pointer_history.append((time.time(), 'arena_free', meta['type'], meta['size'], meta['caller']))
                del pointer_registry[obj_id]
        arena_segments[thread_id].clear()

    @staticmethod
    def debug_overlay():
        print("\n--- MEMORY INSPECTOR OVERLAY ---")
        print(f"Active Allocations: {len(pointer_registry)}")
        print(f"Arena Segments: {len(arena_segments)} threads")
        print(f"Pointer History Length: {len(pointer_history)}")
        snapshot = tracemalloc.take_snapshot()
        top_stats = snapshot.statistics('lineno')[:5]
        print("Top 5 Memory Consumers:")
        for stat in top_stats:
            print(stat)
        print("--------------------------------")

    @staticmethod
    def gc_heatmap():
        heatmap = defaultdict(int)
        for obj_id, meta in pointer_registry.items():
            caller = meta['caller']
            heatmap[caller] += 1
        sorted_heatmap = sorted(heatmap.items(), key=lambda x: -x[1])
        for caller, count in sorted_heatmap:
            print(f"{caller}: {count} allocations")

# Wrap allocation/deallocation to track

def tracked_alloc(obj):
    MemoryInspector.record_allocation(obj)
    return obj

def tracked_free(obj):
    MemoryInspector.record_deallocation(obj)

# CLI entry point
if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Live Memory Inspector')
    parser.add_argument('--snapshot', action='store_true')
    parser.add_argument('--overlay', action='store_true')
    parser.add_argument('--heatmap', action='store_true')
    parser.add_argument('--save', action='store_true')
    parser.add_argument('--arena_free', type=int, help='Thread ID to free arena')
    args = parser.parse_args()

    if args.snapshot:
        MemoryInspector.capture_snapshot()
    if args.overlay:
        MemoryInspector.debug_overlay()
    if args.heatmap:
        MemoryInspector.gc_heatmap()
    if args.save:
        MemoryInspector.save_repl_session()
    if args.arena_free:
        MemoryInspector.arena_free(args.arena_free)
