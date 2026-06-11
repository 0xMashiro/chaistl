# Debugger Support

Debugger support helps inspect `chaistl` containers as logical data structures
instead of raw implementation fields.

This is separate from trace-based visualization:

- Debugger pretty-printers show the current state of a stopped program.
- Trace visualizers show how a state was reached step by step.

## Supported Debuggers

```text
tools/debugger/
  gdb/       GDB Python pretty-printers
  lldb/      LLDB data formatters
  natvis/    Visual Studio Natvis visualizers
```

Only GDB support exists right now. LLDB and Natvis need separate implementations
because their formatter APIs are not compatible with GDB's Python
pretty-printer API.

## Display Contract

Debugger adapters should expose the same logical view across debuggers:

```text
chaistl::vector<T>
  summary: size, capacity
  children: [0], [1], ...

chaistl::array<T, N>
  summary: size
  children: [0], [1], ...
```

When container internals change, update the matching debugger adapter in the
same change.
