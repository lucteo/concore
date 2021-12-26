from cpp_transform._utils import find_tokens, interpret_token_list, token_list_to_str
from clang.cindex import SourceRange


class ReplaceTokens:
    """Replaces the content of an identifier, scanning just tokens"""

    def __init__(self, params):
        self._to = params["to"]
        self._tokens = interpret_token_list(params["tokens"])
        prev = interpret_token_list(_get(params, "prev", []))
        after = interpret_token_list(_get(params, "after", []))
        self._prev_count = len(prev)
        self._expected_tokens = prev + self._tokens + after

    def run(self, unit, verbose):
        """Run this rule on the given C++ unit"""

        if verbose:
            print(
                f"  trying to replace tokens {token_list_to_str(self._tokens)} -> {self._to}..."
            )
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
            loc_start = tokens[idx + self._prev_count].extent.start
            loc_end = tokens[idx + self._prev_count + len(self._tokens) - 1].extent.end
            loc_range = SourceRange.from_locations(loc_start, loc_end)
            unit.add_replacement(loc_range, self._to)
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
