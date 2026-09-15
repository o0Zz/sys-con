"""Just enough ELF64 to read a build ID and the image size.

Both facts come from the same file and neither needs a toolchain binary, so
this stays a pure-Python read:

  * the GNU build ID identifies which binary a crash report belongs to, which
    is what makes an archived ELF usable weeks later;
  * the highest loaded virtual address bounds symbolization. sys-con is linked
    PIE at vaddr 0, so a frame's offset is `pc - module_base`, and any offset
    outside [0, max_vaddr) is not in this binary. Without that check addr2line
    still answers -- with `??` or the nearest preceding symbol -- and a
    confident wrong answer is worse than no answer.
"""

import struct

PT_LOAD = 1
PT_NOTE = 4
SHT_NOTE = 7
NT_GNU_BUILD_ID = 3


class ElfError(Exception):
    pass


class Elf:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        self.path = path

        if self.data[:4] != b"\x7fELF":
            raise ElfError("%s is not an ELF file" % path)
        if self.data[4] != 2:
            raise ElfError("%s is not ELF64" % path)
        if self.data[5] != 1:
            raise ElfError("%s is not little-endian" % path)

        self.phoff = struct.unpack_from("<Q", self.data, 0x20)[0]
        self.phentsize = struct.unpack_from("<H", self.data, 0x36)[0]
        self.phnum = struct.unpack_from("<H", self.data, 0x38)[0]

        self.shoff = struct.unpack_from("<Q", self.data, 0x28)[0]
        self.shentsize = struct.unpack_from("<H", self.data, 0x3A)[0]
        self.shnum = struct.unpack_from("<H", self.data, 0x3C)[0]

    def _segments(self):
        for i in range(self.phnum):
            off = self.phoff + i * self.phentsize
            p_type, _flags, p_offset, p_vaddr, _paddr, p_filesz, p_memsz, _align = \
                struct.unpack_from("<IIQQQQQQ", self.data, off)
            yield p_type, p_offset, p_vaddr, p_filesz, p_memsz

    def max_vaddr(self):
        """One past the highest loaded address, i.e. the image size for a
        binary linked at 0."""
        top = 0
        for p_type, _off, vaddr, _filesz, memsz in self._segments():
            if p_type == PT_LOAD:
                top = max(top, vaddr + memsz)
        if top == 0:
            raise ElfError("%s has no PT_LOAD segments" % self.path)
        return top

    def _sections(self):
        for i in range(self.shnum):
            off = self.shoff + i * self.shentsize
            _name, sh_type, _flags, _addr, sh_offset, sh_size = \
                struct.unpack_from("<IIQQQQ", self.data, off)
            yield sh_type, sh_offset, sh_size

    def _scan_notes(self, offset, size):
        pos, end = offset, offset + size
        while pos + 12 <= end:
            namesz, descsz, ntype = struct.unpack_from("<III", self.data, pos)
            pos += 12
            name = self.data[pos:pos + namesz]
            pos += (namesz + 3) & ~3
            desc = self.data[pos:pos + descsz]
            pos += (descsz + 3) & ~3
            if ntype == NT_GNU_BUILD_ID and name.rstrip(b"\0") == b"GNU":
                return desc.hex()
        return None

    def build_id(self):
        """Lowercase hex GNU build ID, or None when the binary has none.

        devkitA64 links sys-con with no PT_NOTE segment -- the build ID exists
        only as a `.note.gnu.build-id` section -- so the section table is not
        an optional fallback here, it is the path that actually works. The
        segment scan is kept for binaries laid out the usual way.
        """
        for p_type, offset, _vaddr, filesz, _memsz in self._segments():
            if p_type == PT_NOTE:
                found = self._scan_notes(offset, filesz)
                if found:
                    return found

        for sh_type, sh_offset, sh_size in self._sections():
            if sh_type == SHT_NOTE:
                found = self._scan_notes(sh_offset, sh_size)
                if found:
                    return found
        return None
