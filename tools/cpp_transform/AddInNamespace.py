from cpp_transform._utils import (
    visit_cur_unit_ast,
    find_token,
    get_complete_namespace_name,
)
from clang.cindex import CursorKind, TokenKind, SourceRange


class AddInNamespace:
    """Add a text at the beginning of a namespace"""

    def __init__(self, params):
        self._add = params["add"]
        self._in_ns_list = params["in"]

    def run(self, unit, verbose):
        """Run this rule on the given C++ unit"""

        if verbose:
            print(f"  adding in namespace: '{self._add}'...")
        changes_count = 0

        nodes_to_change = []

        def _visit_fun(node):
            # Is this a namespace?
            if node.kind == CursorKind.NAMESPACE:
                # Is this the namespace name to replace?
                ns_name = get_complete_namespace_name(node)
                if ns_name in self._in_ns_list:
                    nodes_to_change.append(node)
                else:
                    # Different namespace, don't recurse in
                    return False

            # Only recurse down for translation units
            # Namespaces can occur at top-level, or within other namespaces
            # But we ignore nested namespaces
            return node.kind == CursorKind.TRANSLATION_UNIT

        # Visit all the AST
        visit_cur_unit_ast(unit.cursor, _visit_fun)

        # Now perform the changes
        for node in nodes_to_change:
            # Get the tokens for the node
            tokens = list(unit.get_tokens(node.extent))
            assert (
                tokens[0].kind == TokenKind.KEYWORD
                and tokens[0].spelling == "namespace"
            )
            # Get the curly token that start the namespace body
            idx = find_token(TokenKind.PUNCTUATION, "{", tokens)

            # Try to get the ident of the first line in the namespace body
            indent = 0
            if idx + 1 < len(tokens):
                next_token = tokens[idx + 1]
                indent = next_token.location.column - 1

            # Add replacement
            loc = tokens[idx].extent.end
            loc_range = SourceRange.from_locations(loc, loc)
            new_code = f"\n{' ' * indent}{self._add}\n"
            unit.add_replacement(loc_range, new_code)
            changes_count += 1

        if verbose and changes_count > 0:
            print(f"    made {changes_count} changes")
