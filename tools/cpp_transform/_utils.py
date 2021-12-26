from clang.cindex import CursorKind, TokenKind


class StopToken:
    """
    Class used to indicate that the AST exploration should completely stop.
    If the visiting function returns an instance of this object, then all the followup nodes will not be explored
    """

    pass


def find_token(kind, text, tokens_list, start_idx=0):
    """
    Find the index of the token with the given kind and text.
    :param kind: The kind of the token we are trying to find
    :param text: The text (or spelling) associated with the token we are trying to find
    :param tokens_list: The list of tokens we are searching into
    :param start_idx: The start index to search from; default=0
    :return: The index of the searched token; -1 if we couldn't find the token.
    """
    n = len(tokens_list)
    for i in range(start_idx, n):
        t = tokens_list[i]
        if t.kind == kind and t.spelling == text:
            return i
    return -1


def find_tokens(expected, tokens_list, start_idx=0):
    """
    Find the starting index of the expected list of tokens.
    :param expected: The list of (kind, text) representing the tokens we are expected to find
    :param tokens_list: The list of tokens we are searching into
    :param start_idx: The start index to search from; default=0
    :return: The index where the expected tokens start in the tokens list; -1 if we could't find the tokens.
    """
    n = len(tokens_list)
    m = len(expected)
    for i in range(start_idx, n - m + 1):
        is_ok = True
        for j in range(m):
            t = tokens_list[i + j]
            ex = expected[j]
            if t.kind != ex[0] or t.spelling != ex[1]:
                is_ok = False
                break
        if is_ok:
            return i
    return -1


def visit_all_ast(node, f):
    """
    Visit all the AST nodes from the translation unit (including nodes from imported headers).
    :param node: The root node from where we start the exploration
    :param f: Functor to be called for the nodes; must match signature (node) -> bool/StopToken

    The functor will be called for each node in the tree, if the exploration is not limited. If the function will return
    False for a node, then the children of that node will not be explored; if True is returned, the children are
    explored.

    If the functor returns a `StopToken` object, then the whole exploration stops.
    """

    def _do_visit(node):
        # Apply the function
        visit_children = f(node)

        # Should we stop?
        if isinstance(visit_children, StopToken):
            return True

        # Visit children
        if visit_children:
            for c in node.get_children():
                should_stop = _do_visit(c)
                if should_stop:
                    return True

    # Start the exploration
    _do_visit(node)


def visit_cur_unit_ast(node, f):
    """
    Visit the AST nodes from the current file (ignoring nodes from imported headers).
    :param node: The root node from where we start the exploration
    :param f: Functor to be called for the nodes; must match signature (node) -> bool/StopToken

    The functor will be called for each node in the tree, if the exploration is not limited. If the function will return
    False for a node, then the children of that node will not be explored; if True is returned, the children are
    explored.

    If the functor returns a `StopToken` object, then the whole exploration stops.

    All the nodes that do not belong to the file of the starting node will be ignored.
    """
    our_file = node.extent.start.file

    def _do_visit(node):
        # Don't visit nodes from other files
        if node.location.file and node.location.file.name != our_file.name:
            return False

        # Apply the function
        visit_children = f(node)

        # Should we stop?
        if isinstance(visit_children, StopToken):
            return True

        # Visit children
        if visit_children:
            for c in node.get_children():
                should_stop = _do_visit(c)
                if should_stop:
                    return True

    # Start the exploration
    _do_visit(node)


def get_complete_namespace_name(node):
    """
    Gets the complete name for a namespace given the start ode
    :param node: The node of the namespace
    :return: The complete (i.e., qualified name) of the namespace

    If we have a namespace like "A::B::C", in the AST this will be represented as 3 different namespace nodes. This
    function will check for this nesting and build the original namespace name
    """
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


def interpret_token_list(params_list):
    """
    Given a list coming from the configuration yaml file, transform it into a list of pairs (TokenKind, text)
    :param params_list: the list coming from the configuration file
    :return: corresponding list containing (TokenKind, text) pairs
    """
    res = []
    for el in params_list:
        kind = TokenKind.IDENTIFIER
        if el["token"] == "punct":
            kind = TokenKind.PUNCTUATION
        elif el["token"] == "kwd":
            kind = TokenKind.KEYWORD
        p = (kind, el["text"])
        res.append(p)
    return res


def token_list_to_str(tok_list):
    """
    Convert a token list to string, for displaying
    :param tok_list: The token list; list of (TokenKind, text) paris
    :return: The human-readable string
    """
    res = ""
    prev_kind = TokenKind.PUNCTUATION
    for t in tok_list:
        if t[0] != TokenKind.PUNCTUATION and prev_kind != TokenKind.PUNCTUATION:
            res += " "
        prev_kind = t[0]
        res += t[1]
    return res
