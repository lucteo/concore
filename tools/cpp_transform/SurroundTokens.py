from cpp_transform._utils import find_tokens
from clang.cindex import CursorKind, TokenKind, SourceRange


class SurroundTokens:
    """Surround the given set of tokens with some strings; usefull for adding #ifdefs"""

    def __init__(self, params):
        self._pre = params["pre"]
        self._post = params["post"]
        self._tokens = _interpret_token_list(params["tokens"])

    def run(self, unit, verbose):
        """Run this rule on the given C++ unit"""

        if verbose:
            print(f"  surrounding tokens '{token_list_to_str(self._tokens)}'...")
        changes_count = 0

        all_tokens = list(unit.get_tokens())

        n = len(all_tokens)
        start = 0
        while start < n:
            # Can we find our expected tokens?
            idx = find_tokens(self._tokens, all_tokens, start)
            if idx < 0:
                break

            # Add code before the first token
            loc_start = all_tokens[idx].extent.start
            loc_end = all_tokens[idx + len(self._tokens)-1].extent.end
            if self._pre:
                loc_range = SourceRange.from_locations(loc_start, loc_start)
                unit.add_replacement(loc_range, self._pre)
            if self._post:
                loc_range = SourceRange.from_locations(loc_end, loc_end)
                unit.add_replacement(loc_range, self._post)

            if self._pre or self._post:
                changes_count += 1

            # Continue from the next token
            start = idx + len(self._tokens) + 1

        if verbose and changes_count > 0:
            print(f"    made {changes_count} changes")


def token_list_to_str(tok_list):
    res = ""
    prev_kind = TokenKind.PUNCTUATION
    for t in tok_list:
        if t[0] == TokenKind.IDENTIFIER and prev_kind == TokenKind.IDENTIFIER:
            res += " "
        res += t[1]
    return res


def _interpret_token_list(params_list):
    res = []
    for el in params_list:
        kind = TokenKind.IDENTIFIER
        if el["token"] == "punct":
            kind = TokenKind.PUNCTUATION
        p = (kind, el["text"])
        res.append(p)
    return res
