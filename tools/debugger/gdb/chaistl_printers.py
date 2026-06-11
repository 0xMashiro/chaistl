# GDB pretty-printers for chaistl.
#
# References:
# - GDB Python API pretty-printer documentation.
# - libstdc++ ships similar GDB pretty-printers for production STL containers.
#
# These printers are written for chaistl's current learning implementation. They
# intentionally expose logical container state while relying on the container's
# documented internal invariants.

from __future__ import annotations

import re
from typing import Iterator, Tuple

import gdb
import gdb.printing


def _strip_type(type_: gdb.Type) -> gdb.Type:
    return type_.strip_typedefs().unqualified()


def _type_name(value: gdb.Value) -> str:
    type_ = _strip_type(value.type)
    return type_.tag or str(type_)


class ChaistlVectorPrinter:
    """Pretty-printer for chaistl::vector<T, Allocator>."""

    def __init__(self, value: gdb.Value):
        self.value = value
        self.first = value["first_"]
        self.last = value["last_"]
        self.capacity_last = value["capacity_last_"]

    def display_hint(self) -> str:
        return "array"

    def _size(self) -> int:
        return int(self.last - self.first)

    def _capacity(self) -> int:
        return int(self.capacity_last - self.first)

    def children(self) -> Iterator[Tuple[str, gdb.Value]]:
        first = self.first
        for index in range(self._size()):
            yield f"[{index}]", first[index]

    def to_string(self) -> str:
        return f"{_type_name(self.value)} size={self._size()} capacity={self._capacity()}"


class ChaistlArrayPrinter:
    """Pretty-printer for chaistl::array<T, N>."""

    def __init__(self, value: gdb.Value):
        self.value = value
        self.elements = value["elements"]
        self.size = self._read_size()

    def display_hint(self) -> str:
        return "array"

    def _read_size(self) -> int:
        try:
            return int(_strip_type(self.value.type).template_argument(1))
        except Exception:
            pass

        match = re.search(r"chaistl::array(?:@[^<]+)?<[^,]+,\s*([0-9]+)\s*>", _type_name(self.value))
        if match:
            return int(match.group(1))

        array_type = _strip_type(self.elements.type)
        try:
            return int(array_type.range()[1]) + 1
        except Exception:
            return 0

    def children(self) -> Iterator[Tuple[str, gdb.Value]]:
        for index in range(self.size):
            yield f"[{index}]", self.elements[index]

    def to_string(self) -> str:
        return f"{_type_name(self.value)} size={self.size}"


def build_pretty_printer() -> gdb.printing.RegexpCollectionPrettyPrinter:
    printer = gdb.printing.RegexpCollectionPrettyPrinter("chaistl")
    printer.add_printer("chaistl::vector", r"^chaistl::vector(?:@[^<]+)?<.*>$", ChaistlVectorPrinter)
    printer.add_printer("chaistl::array", r"^chaistl::array(?:@[^<]+)?<.*>$", ChaistlArrayPrinter)
    return printer


def register_chaistl_printers(objfile=None) -> None:
    """Register chaistl pretty-printers with GDB.

    Pass a GDB objfile to scope registration to one loaded object. If omitted,
    printers are registered on the current object file or globally as a fallback.
    """

    target = objfile or gdb.current_objfile() or gdb.current_progspace()
    gdb.printing.register_pretty_printer(target, build_pretty_printer(), replace=True)


register_chaistl_printers()
