pointer_history = []

def log_pointer_event(ptr, action, type_tag, size, caller_fn):
    pointer_history.append({
        'timestamp': time.time(),
        'action': action,
        'pointer': ptr,
        'type_tag': type_tag,
        'size': size,
        'caller': caller_fn,
        'stack': traceback.format_stack(limit=5)
    })

def allocate_logged(size, type_tag):
    ptr = low_level_alloc(size)
    log_pointer_event(ptr, 'alloc', type_tag, size, get_caller())
    return ptr

def free_logged(ptr):
    log_pointer_event(ptr, 'free', pointer_registry[ptr].type_tag, 0, get_caller())
    low_level_free(ptr)
