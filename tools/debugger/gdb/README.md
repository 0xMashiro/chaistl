# chaistl GDB Pretty-Printers

This directory contains GDB pretty-printers for inspecting `chaistl` containers
while debugging.

Pretty-printers are debugger-side adapters. They do not change the library and
do not participate in program execution. Their job is to translate implementation
details such as raw storage pointers into logical container views.

## Supported Types

- `chaistl::vector<T, Allocator>`
- `chaistl::array<T, N>`

## Load Manually

From GDB:

```gdb
source /path/to/chaistl/tools/debugger/gdb/chaistl_printers.py
```

Then print a value normally:

```gdb
p values
```

A `chaistl::vector<int>` should display as a logical array with a summary similar
to:

```text
chaistl::vector<int> size=3 capacity=8 = {[0] = 1, [1] = 2, [2] = 3}
```

## VS Code

Add the source command to the debugger setup commands:

```json
{
  "setupCommands": [
    {
      "text": "source ${workspaceFolder}/tools/debugger/gdb/chaistl_printers.py"
    }
  ]
}
```

## Design Notes

The printers are intentionally kept outside `src/chaistl` because they are
tooling, not library code. They rely on current container invariants:

- `vector` uses `first_`, `last_`, and `capacity_last_`.
- `array` stores elements in `elements`.

If those internals change, update the printers in the same change.
