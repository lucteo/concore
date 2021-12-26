import clang.cindex
import sys
import os


class TransformedUnit:
    """
    Represents a C++ translation unit, possible with transformations applied to it.
    Multiple transformations passes can be applied to a single unit, however the transformations must not change the
    same code multiple times.
    This offers the ability to traverse the unit, and apply more transformations.
    At the end, the unit can be saved to disk.
    """

    def __init__(self, filename, clang_args):
        """Create a transformed unit from the given filename, parsing it with the given clang arguments"""
        # Read the contents of the file
        with open(filename) as f:
            self._contents = f.read()
        self._filename = filename

        # Parse the file with libclang; treat the file as an unsaved file
        self._idx = clang.cindex.Index.create()
        self._clang_args = clang_args
        self._parse()
        # We store here all the replacements we need to make
        # list of (offset start, offset end, text)
        self.replacements = []

    def save(self, filename):
        """Saves the content of the transformed unit to disk."""
        with open(filename, "w") as f:
            new_content = self._get_modified_content()
            f.write(new_content)

    def add_replacement(self, loc_range, new_text):
        """Add a replacement text for the characters indicated by the given SourceLocation"""
        rstart = loc_range.start.offset
        rend = loc_range.end.offset
        assert rend > rstart
        assert os.path.samefile(
            loc_range.start.file.name, self._filename
        ), f"Invalid location filename: {loc_range.start.file} != {self._filename}"
        tup = (rstart, rend, new_text)
        self.replacements.append(tup)

    def apply_and_reparse(self):
        """
        Apply the transformations and reparse the source file; to be used when multiple transformations might touch
        the same tokens.
        """
        self._contents = self._get_modified_content()
        self.replacements = []
        self._parse()

    def get_tokens(self, extent=None):
        """
        Returns the tokens in the specified extent for the current unit (lexical analysis only).
        If no extent is specified, we retrieve the tokens for the entire file.
        """
        if not extent:
            extent = self._tu.cursor.extent
        return self._tu.get_tokens(extent=extent)

    @property
    def tu(self):
        """Returns the libclang translation unit object"""
        return self._tu

    @property
    def includes(self):
        """Returns the includes of the unit; the first include is the unit itself"""
        return self._tu.get_includes()

    @property
    def cursor(self):
        """Returns the top-leve cursor, representing the whole AST of the unit"""
        return self._tu.cursor

    def _get_modified_content(self):
        """Applies the replacements and get the modified content"""
        if not self.replacements:
            return self._contents

        replacements = sorted(self.replacements)

        cur = 0
        result = ""
        for (rstart, rend, text) in replacements:
            if rstart < cur:
                print(
                    f"WARNING: overlapping replacements (when writing '{text}')",
                    file=sys.stderr,
                )
                continue
            result += self._contents[cur:rstart]
            result += text
            cur = rend
        result += self._contents[cur:]
        return result

    def _parse(self):
        """Parses the contents of the filename"""
        self._tu = self._idx.parse(
            self._filename,
            args=self._clang_args,
            unsaved_files=[(self._filename, self._contents)],
            options=0,
        )
