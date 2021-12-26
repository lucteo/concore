import sys
from cpp_transform._utils import visit_cur_unit_ast, find_token
from clang.cindex import CursorKind, TokenKind, SourceRange, SourceLocation


class NamespaceRename:
    """Renames namespaces according to the given parameters; namespaces need to be top-level"""

    def __init__(self, params):
        for k, v in params.items():
            if k == "from":
                self._from = v
                self._from_num_parts = v.count("::") + 1
            elif k == "to":
                self._to = v
            else:
                print(
                    f"ERROR: invalid parameter {k} in NamespaceRename", file=sys.stderr
                )

    def run(self, unit, verbose):
        """Run this rule on the given C++ unit"""

        if verbose:
            print(f"  trying namespace rename {self._from} -> {self._to}...")
        replacements_count = 0

        nodes_to_replace = []

        def _visit_fun(node):
            # Is this a namespace?
            if node.kind == CursorKind.NAMESPACE:
                # Is this the namespace name to replace?
                ns_name = _get_complete_namespace_name(node)
                if ns_name == self._from:
                    nodes_to_replace.append(node)
                else:
                    # Different namespace, don't recurse in
                    return False

            # Only recurse down for translation units
            # Namespaces can occur at top-level, or within other namespaces
            # But we ignore nested namespaces
            return node.kind == CursorKind.TRANSLATION_UNIT

        # Visit all the AST
        visit_cur_unit_ast(unit.cursor, _visit_fun)

        # Now perform the replacements
        for node in nodes_to_replace:
            # Get the tokens for the node
            tokens = list(unit.get_tokens(node.extent))
            assert (
                tokens[0].kind == TokenKind.KEYWORD
                and tokens[0].spelling == "namespace"
            )
            idx = find_token(TokenKind.PUNCTUATION, "{", tokens)
            assert idx == self._from_num_parts * 2

            # Add replacement
            start_loc = tokens[1].location
            end_loc = tokens[idx - 1].extent.end
            loc_range = SourceRange.from_locations(start_loc, end_loc)
            unit.add_replacement(loc_range, self._to)
            replacements_count += 1

            # Sometimes, after the namespace we have a comment with the name of the namespace
            # Try to rename this as well
            start_loc = node.extent.end
            end_loc = SourceLocation.from_position(
                unit.tu, start_loc.file, start_loc.line + 2, 1
            )
            loc_range = SourceRange.from_locations(start_loc, end_loc)
            tokens = unit.get_tokens(loc_range)
            t = next(tokens, None)
            if t and t.kind == TokenKind.COMMENT and self._from in t.spelling:
                new_comment = t.spelling.replace(self._from, self._to)
                if new_comment != t.spelling:
                    unit.add_replacement(t.extent, new_comment)

        if verbose and replacements_count > 0:
            print(f"    made {replacements_count} replacements")


def _get_complete_namespace_name(node):
    full_name = node.spelling
    end_loc = node.extent.end
    # Recursively check if we have connected namespaces, i.e., A::B::C
    can_continue = True
    while can_continue:
        can_continue = False
        children = list(node.get_children())
        # If we have only one child
        if len(children) == 1:
            child = children[0]
            # ... and the child is a namespace that ends at the exactly same position
            if child.kind == CursorKind.NAMESPACE and child.extent.end == end_loc:
                # ... then this is part of a compound namespace name
                full_name += f"::{child.spelling}"
                can_continue = True
                node = child
    return full_name
