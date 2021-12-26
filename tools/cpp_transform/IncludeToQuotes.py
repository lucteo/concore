from cpp_transform._utils import find_token, find_tokens
from clang.cindex import TokenKind, SourceRange


class IncludeToQuotes:
    """Converts includes from <header> to "header" form"""

    def __init__(self, params):
        self.headers = params

    def run(self, unit, verbose):
        """Run this rule on the given C++ unit"""

        if verbose:
            print(f"  trying changing includes to quotes...")
        replacements_count = 0

        tokens = list(unit.get_tokens())

        expected_start = [
            (TokenKind.PUNCTUATION, "#"),
            (TokenKind.IDENTIFIER, "include"),
            (TokenKind.PUNCTUATION, "<"),
        ]

        n = len(tokens)
        start = 0
        while start < n:
            # Can we find any "#include<" tokens?
            idx = find_tokens(expected_start, tokens, start)
            if idx < 0:
                break

            # Find the ">" token
            idx2 = find_token(TokenKind.PUNCTUATION, ">", tokens, idx + 3)

            # Merge the text for the tokens of the include filename
            include = ""
            for i in range(idx + 3, idx2):
                include += tokens[i].spelling
            if include in self.headers:
                start_loc = tokens[idx + 2].location
                end_loc = tokens[idx2].extent.end
                loc_range = SourceRange.from_locations(start_loc, end_loc)
                unit.add_replacement(loc_range, '"' + include + '"')
                replacements_count += 1

            # Continue from the token after this include
            start = idx2 + 1

        if verbose and replacements_count > 0:
            print(f"    made {replacements_count} replacements")
