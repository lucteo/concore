from cpp_transform._utils import find_tokens, interpret_token_list
from clang.cindex import TokenKind


class TokenIdReplace:
    """Replaces the content of an identifier, scanning just tokens"""

    def __init__(self, params):
        self._from = params["from"]
        self._from_num_parts = self._from.count("::") + 1
        self._to = params["to"]
        prev = interpret_token_list(_get(params, "prev", []))
        after = interpret_token_list(_get(params, "after", []))
        self._prev_count = len(prev)
        self._expected_tokens = prev + [(TokenKind.IDENTIFIER, self._from)] + after

    def run(self, unit, verbose):
        """Run this rule on the given C++ unit"""

        if verbose:
            print(f"  trying replace identifier token {self._from} -> {self._to}...")
        replacements_count = 0

        tokens = list(unit.get_tokens())

        n = len(tokens)
        start = 0
        while start < n:
            # Can we find our expected tokens?
            idx = find_tokens(self._expected_tokens, tokens, start)
            if idx < 0:
                break

            # Perform the replacement of our token
            replace_token = tokens[idx + self._prev_count]
            unit.add_replacement(replace_token.extent, self._to)
            replacements_count += 1

            # Continue from the next token
            start = idx + self._prev_count + 1

        if verbose and replacements_count > 0:
            print(f"    made {replacements_count} replacements")


def _get(d, key, default):
    if key in d:
        return d[key]
    else:
        return default
