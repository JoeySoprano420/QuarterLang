class SnapshotDebugger:
    def __init__(self):
        self.snapshots = []

    def take_snapshot(self, vm):
        frame_state = vm.capture_frame_state()
        memory_state = dump_heap_memory()
        self.snapshots.append({
            'frame': frame_state,
            'heap': memory_state,
            'time': time.time()
        })

    def replay_snapshot(self, index, vm):
        snap = self.snapshots[index]
        vm.load_frame_state(snap['frame'])
        load_heap_memory(snap['heap'])
